Name:       flashlight-sysfs
Summary:    Sailfish OS flashlight provider for a sysfs torch node
Version:    0.1.1
Release:    1
License:    BSD-3-Clause
URL:        https://github.com/nyxianglyph/flashlight-sysfs
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(glib-2.0)
Provides:   sailfish-flashlight-provider
Conflicts:  droidmedia-flashlight
Obsoletes:  droidmedia-flashlight

%description
Implements the org.sailfish.flashlight.provider D-Bus service by writing to
a sysfs torch node, for devices where the Android side cannot switch the
torch.

%prep
%autosetup -n %{name}-%{version}

%build
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%defattr(-,root,root,-)
%license LICENSE
%{_bindir}/flashlight-sysfs
%config(noreplace) %{_sysconfdir}/flashlight-sysfs.conf
%{_datadir}/dbus-1/services/org.sailfish.flashlight.provider.service
/usr/libexec/flashlight-sysfs-permissions
/usr/lib/systemd/system/flashlight-sysfs-permissions.service
/usr/lib/systemd/system/multi-user.target.wants/flashlight-sysfs-permissions.service
