#! /usr/bin/perl

$otlup="/usr/mnews/lib/otlup.txt";
$ENV{"PATH"}="/bin:/usr/bin:/sbin:/usr/sbin:/usr/mnews/bin:/usr/lib/news/bin";
@goodusers=qw(
	nata@nata.nauu.kiev.ua
	gvs@rinet.ru
	serg@ritlabs.com
	igorm@energoinfo.hu
	gerelo@cam.org
	gundar@usf.kiev.ua
);
@gooddomains=qw(
	astral.kiev.ua
	avico.kiev.ua
	ulysses.kiev.ua
	ant.kiev.ua
	ppro.kiev.ua
	unian.net
	ora.iptelecom.net.ua
);
@badmx=qw(
	proline\.kiev\.ua$
	barents\.kiev\.ua$
	webber
	sabbo\.kiev\.ua$
	brovary\.kiev\.ua$
	elvisti\.kiev\.ua$
	icyb\.kiev\.ua$
	nest\.vinnica\.ua$
	cii\.sumy\.ua$
);
@goodmx=qw(
	carrier\.kiev\.ua$
	lucky\.net$
	sita\.kiev\.ua$
	frigate\.kiev\.ua$
	execrel\.kiev\.ua$
	uninet\.kiev\.ua$
	magnus\.net\.ua$
);

#open LOG, ">>/var/log/mailnews/badusers.log" || die "Can't open log: $!";

$domain=$ARGV[0]; 
if ($ARGV[0] =~ /^([a-z0-9.\-_\!\~]+)\@([a-z0-9.\-_]+)$/i)
{	$user = $1;
	$domain = $2;
}
else
{
	print LOG "$ARGV[0] - bad address\n";
	print STDERR "$ARGV[0] - bad address\n" if @ARGV>1;
	exit 1;
}
if ($domain eq "kozlik.carrier.kiev.ua" && $user =~ /\!([a-z0-9.\-_]+)\!([a-z0-9.\-_]+)$/i)
{
	$domain = $1;
	$user = $2;
	$newdomain = $domain;
	$newuser = $user;
}

$domain =~ tr/[A-Z]/[a-z]/;

if ($domain eq "kozlik.carrier.kiev.ua" || $user =~ /!/)
{
	print LOG "$ARGV[0] - bad address\n";
	print STDERR "$ARGV[0] - bad address\n" if @ARGV>1;
	exit 1;
}

if ($domain =~ /^(unicorn\.)?carrier\.kiev\.ua$/)
{
	$user = $ARGV[0];
	$user =~ s/\@carrier\.kiev\.ua$/\@mail.carrier.kiev.ua/i;
	open OUT, "/usr/bin/finger $user |" || die "Cannot run finger: $!";
#	unless (open OUT, "/usr/bin/finger $user |")
#	{
#		print LOG "Can't run finger $user: $!\n";
#		die "Cannot run finger: $!";
#	}
	@out = grep(/no such user/, <OUT>);
	close OUT;
	if (@out > 0)
	{
#		print LOG "$ARGV[0] - bad local user\n";
		print STDERR "$ARGV[0] - bad local user\n" if @ARGV>1;
		exit 1;
	}
#	print LOG "$ARGV[0] - good user\n";
	print STDOUT $newuser . "\@" . $newdomain . "\n" if $newdomain;
	exit 0;
}

$_ = $user . "\@" . $domain;
tr/[A-Z]/[a-z]/;
foreach $user (@goodusers)
{
	if ($_ eq $user) {
	    print STDOUT $newuser . "\@" . $newdomain . "\n" if $newdomain;
	    exit 0;
	}
}
foreach $user (@gooddomains)
{
	if ($domain eq $user) {
	    print STDOUT $newuser . "\@" . $newdomain . "\n" if $newdomain;
	    exit 0;
	}
}

#printf LOG "Running /usr/bin/nslookup -type=mx $domain 2>&1, PATH=%s\n", $ENV{"PATH"};

open OUT, "/usr/bin/dig $domain mx 2>&1 |" || die "Cannot exec dig: $!";
$gotanswer=0;
while (<OUT>)
{
	chomp();
	next if /^(;.*)?$/;
	$gotanswer=1;
	if (/\sIN\s+MX\s+([0-9]+)\s+(\S+)\.$/)
	{	$mxlist{$2}=$1 unless (defined($mxlist{$2}) && $mxlist{$2}<$1);
	}
}
close OUT;
#print LOG "dig exited\n";
unless ($gotanswer)
{
	print STDERR "Warning: $ARGV[0] - no NS available\n" if @ARGV>1;
	exit 0;
}
if (%mxlist == 0)
{
#	print LOG "$ARGV[0] - bad domain (no MX's)\n";
	print STDERR "$ARGV[0] - bad domain (no MX's)\n" if @ARGV>1;
	exit 1;
}
@mxlist = sort { $mxlist{$a} <=> $mxlist{$b} } keys %mxlist;
#print LOG "less MX is $mxlist[0]\n";

$_=$domain;
foreach $mx (@badmx)
{
	otlup() if $mxlist[0] =~ /$mx/i;
}
foreach $mx (@goodmx)
{
	exit 0 if $mxlist[0] =~ /$mx/i;
	if (@mxlist>1)
	{
		if ($mxlist[1] =~ /$mx/i)
		{
			print STDOUT $newuser . "\@" . $newdomain . "\n" if $newdomain;
			exit 0;
		}
	}
}

otlup();

sub otlup
{
#	print LOG "$ARGV[0] - bad user\n";
	if (@ARGV>1)
	{
		print STDERR "$ARGV[0] - bad user\n";
	}
	else
	{
		system("cat $otlup | mail -s \"Access to news\@lucky.net denied\" @ARGV[0]");
	}
	exit 1;
}

