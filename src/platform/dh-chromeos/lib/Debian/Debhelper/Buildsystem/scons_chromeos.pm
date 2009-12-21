# A debhelper build system class for handling SCons based projects.
# Extended for Chrome OS (cross-compiling support, parallel builds).
#
# Copyright: © 2009 Luca Falavigna
# Copyright: © 2009 The Chromium OS Authors.
# License: GPL-2+

package Debian::Debhelper::Buildsystem::scons_chromeos;

use strict;
use File::Path;
use Debian::Debhelper::Dh_Lib qw(dpkg_architecture_value);
use base 'Debian::Debhelper::Buildsystem';

sub DESCRIPTION {
	"SCons, extended for Chrome OS (Sconstruct)"
}

sub check_auto_buildable {
	my $this=shift;
	if (-e $this->get_sourcepath("SConstruct")) {
		return -e $this->get_sourcepath("SConstruct");
	} elsif (-e $this->get_sourcepath("Sconstruct")) {
		return -e $this->get_sourcepath("Sconstruct");
	} elsif (-e $this->get_sourcepath("sconstruct")) {
		return -e $this->get_sourcepath("sconstruct");
	}
}

sub new {
	my $class=shift;
	my $this=$class->SUPER::new(@_);
	$this->enforce_in_source_building();
	return $this;
}

my %toolchain = (
	CC => 'gcc',
	CXX => 'g++',
	AR => 'ar',
	RANLIB => 'ranlib',
	LD => 'ld',
	NM => 'nm',
);

sub build {
	my $this=shift;

	my $deb_build_gnu_type=dpkg_architecture_value("DEB_BUILD_GNU_TYPE");
	my $deb_host_gnu_type=dpkg_architecture_value("DEB_HOST_GNU_TYPE");
	if ($deb_build_gnu_type eq $deb_host_gnu_type) {
		for my $tool (keys %toolchain) {
			$ENV{$tool}=$toolchain{$tool};
		}
	} else {
		for my $tool (keys %toolchain) {
			$ENV{$tool}="$deb_host_gnu_type-$toolchain{$tool}";
		}
	}

	my @opts;
	open NUM_JOBS, '-|',
		"grep processor /proc/cpuinfo | awk '{a++} END {print a}'";
	my $num_jobs = <NUM_JOBS>;
	chomp $num_jobs;
	close NUM_JOBS;
	if ($num_jobs ne '') {
		push @opts, "-j$num_jobs";
	}

	$this->doit_in_sourcedir("scons", @opts, @_);
}

sub clean {
	my $this=shift;
	$this->doit_in_sourcedir("scons", "-c", @_);
	unlink($this->get_buildpath(".sconsign.dblite"));
	rmtree($this->get_buildpath(".sconf_temp"));
}

1
