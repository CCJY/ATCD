eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use lib "$ENV{ACE_ROOT}/bin";
use PerlACE::TestTarget;

my $exit_status = 0;

my $server = PerlACE::TestTarget::create_target (1) || die "Create target 1 failed\n";
my $client = PerlACE::TestTarget::create_target (2) || die "Create target 2 failed\n";

my $server_conf_base = "mt_noupcall$PerlACE::svcconf_ext";
my $server_conf = $server->LocalFile ($server_conf_base);
if ($server->PutFile ($server_conf_base) == -1) {
    print STDERR "ERROR: cannot set file <$server_conf>\n";
    exit 1;
}

$debug_level = '0';
foreach $i (@ARGV) {
    if ($i eq '-d') {
        $debug_level = '10';
    }
}

my $debugarg = "-ORBDebugLevel $debug_level -ORBVerboseLogging 1 -ORBLogFile ";
my $server_debug = $debugarg . " server.log" if ($debug_level > 0);
my $client_debug = $debugarg . " client.log" if ($debug_level > 0);

my $server_iorbase = "server.ior";
my $server_iorfile = $server->LocalFile ($server_iorbase);

$server->DeleteFile($server_iorbase);
$server->DeleteFile("server.log");
$client->DeleteFile("client.log");


$SV = $server->CreateProcess ("server", "-ORBSvcConf $server_conf_base $server_debug");
$server_status = $SV->Spawn ();
if ($server_status != 0) {
    print STDERR "ERROR: server returned $server_status\n";
    exit 1;
}
if ($server->WaitForFileTimed ($server_iorbase,
                               $server->ProcessStartWaitInterval()) == -1) {
    print STDERR "ERROR: cannot find file <$server_iorfile>\n";
    $SV->Kill (); $SV->TimedWait (1);
    exit 1;
}

$CL = $client->CreateProcess ("client", "$client_debug");
$client_status = $CL->SpawnWaitKill ($client->ProcessStartWaitInterval() + 300);
if ($client_status != 0) {
    print STDERR "ERROR: client returned $client_status\n";
    $exit_status = 1;
}

print "INFO: Awaiting server ...\n";
$server_status = $SV->WaitKill ($server->ProcessStopWaitInterval());
if ($server_status != 0) {
    print STDERR "ERROR: server returned $server_status\n";
    $exit_status = 1;
}

$server->DeleteFile($server_iorfile);

if ($exit_status == 0) {
  print "INFO: Test succeeded\n";
}
else {
  print "INFO: Test Failed!\n";
}

exit $exit_status;
