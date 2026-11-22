/*
 * Flashlight provider for Sailfish OS that drives a sysfs torch node.
 *
 * Implements the D-Bus API of droidmedia-flashlight
 * (org.sailfish.flashlight.provider on the session bus) for devices where
 * the Android side cannot switch the torch but the kernel exposes it.
 *
 * Configuration: /etc/flashlight-sysfs.conf, key=value lines
 *   path=<sysfs file>   file to write to
 *   on=<value>          value that turns the torch on
 *   off=<value>         value that turns it off
 */
#include <gio/gio.h>
#include <glib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUS_NAME  "org.sailfish.flashlight.provider"
#define OBJ_PATH  "/org/sailfish/flashlight/provider"
#define IFACE     "org.sailfish.flashlight.provider"
#define CONF_FILE "/etc/flashlight-sysfs.conf"
#define IDLE_EXIT_SECONDS 30

static const char introspection_xml[] =
    "<node>"
    "  <interface name='" IFACE "'>"
    "    <property name='flashlightOn' type='b' access='read'/>"
    "    <signal name='flashlightOnChanged'>"
    "      <arg name='newOn' type='b'/>"
    "    </signal>"
    "    <method name='toggleFlashlight'>"
    "      <arg type='b' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static char *conf_path;
static char *conf_on;
static char *conf_off;
static gboolean torch_on;
static GMainLoop *loop;
static guint idle_source;

static void load_config(void)
{
    char *data = NULL;

    conf_path = g_strdup("/sys/class/flashlight_core/flashlight/torchbrightness");
    conf_on = g_strdup("100");
    conf_off = g_strdup("0");
    if (!g_file_get_contents(CONF_FILE, &data, NULL, NULL))
        return;
    char **lines = g_strsplit(data, "\n", -1);
    for (char **l = lines; *l; l++) {
        char *line = g_strstrip(*l);
        char *eq = strchr(line, '=');
        if (!*line || *line == '#' || !eq)
            continue;
        *eq = '\0';
        char *key = g_strstrip(line), *value = g_strstrip(eq + 1);
        if (!strcmp(key, "path")) { g_free(conf_path); conf_path = g_strdup(value); }
        else if (!strcmp(key, "on")) { g_free(conf_on); conf_on = g_strdup(value); }
        else if (!strcmp(key, "off")) { g_free(conf_off); conf_off = g_strdup(value); }
    }
    g_strfreev(lines);
    g_free(data);
}

static gboolean write_torch(gboolean on)
{
    const char *value = on ? conf_on : conf_off;
    int fd = open(conf_path, O_WRONLY | O_CLOEXEC);
    gboolean ok;

    if (fd < 0) {
        g_warning("cannot open %s: %s", conf_path, g_strerror(errno));
        return FALSE;
    }
    ok = write(fd, value, strlen(value)) == (ssize_t)strlen(value);
    if (!ok)
        g_warning("cannot write to %s: %s", conf_path, g_strerror(errno));
    close(fd);
    return ok;
}

static gboolean idle_exit(gpointer data)
{
    idle_source = 0;
    if (!torch_on)
        g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

/* The service is bus activated; it only has to stay while the torch is on. */
static void schedule_idle_exit(void)
{
    if (idle_source) {
        g_source_remove(idle_source);
        idle_source = 0;
    }
    if (!torch_on)
        idle_source = g_timeout_add_seconds(IDLE_EXIT_SECONDS, idle_exit, NULL);
}

static void set_torch(GDBusConnection *conn, gboolean on)
{
    if (on == torch_on || !write_torch(on)) {
        schedule_idle_exit();
        return;
    }
    torch_on = on;
    g_dbus_connection_emit_signal(conn, NULL, OBJ_PATH, IFACE, "flashlightOnChanged",
        g_variant_new("(b)", torch_on), NULL);

    GVariantBuilder changed;
    g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&changed, "{sv}", "flashlightOn", g_variant_new_boolean(torch_on));
    g_dbus_connection_emit_signal(conn, NULL, OBJ_PATH, "org.freedesktop.DBus.Properties",
        "PropertiesChanged", g_variant_new("(sa{sv}as)", IFACE, &changed, NULL), NULL);
    schedule_idle_exit();
}

static void on_method_call(GDBusConnection *conn, const gchar *sender, const gchar *path,
    const gchar *iface, const gchar *method, GVariant *params,
    GDBusMethodInvocation *invocation, gpointer user_data)
{
    if (!g_strcmp0(method, "toggleFlashlight")) {
        set_torch(conn, !torch_on);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", torch_on));
    } else {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method %s", method);
    }
}

static GVariant *on_get_property(GDBusConnection *conn, const gchar *sender, const gchar *path,
    const gchar *iface, const gchar *property, GError **error, gpointer user_data)
{
    if (!g_strcmp0(property, "flashlightOn"))
        return g_variant_new_boolean(torch_on);
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property %s", property);
    return NULL;
}

static const GDBusInterfaceVTable vtable = { on_method_call, on_get_property, NULL, { 0 } };

static void on_bus_acquired(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    GError *error = NULL;

    if (!g_dbus_connection_register_object(conn, OBJ_PATH, info->interfaces[0], &vtable,
            NULL, NULL, &error)) {
        g_warning("cannot register object: %s", error->message);
        g_main_loop_quit(loop);
    }
    g_dbus_node_info_unref(info);
}

static void on_name_lost(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    g_main_loop_quit(loop);
}

int main(void)
{
    load_config();
    loop = g_main_loop_new(NULL, FALSE);
    g_bus_own_name(G_BUS_TYPE_SESSION, BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE,
        on_bus_acquired, NULL, on_name_lost, NULL, NULL);
    schedule_idle_exit();
    g_main_loop_run(loop);
    /* Never leave the LED on behind us. */
    if (torch_on)
        write_torch(FALSE);
    return 0;
}
