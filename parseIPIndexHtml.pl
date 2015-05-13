#!/usr/bin/perl -w

use strict;
use DBI;

my $DSN = "dbi:mysql:dbname=poesis";

my $user = "razvan";
my $pass = "iMOri77ss";
my $dbh;
my $sth;

@ARGV = ("ip-index-html");

my ($IP, $title, $status);

my $bigstring;

my @lines = <>;

if($#lines >= 2){
	$IP = $lines[0];
	$status = $lines[1];
	if($status =~ /200 OK/){
		foreach (@lines){
			chomp;
			$bigstring .= $_;
		}
		if($bigstring =~ /<title>(.+)<\/title>/i){
			$title = $1;
		}
		if(defined($title)){
			print "\tIP = $IP";
			print "\tTitle = $title\n";
			
			$dbh = DBI->connect($DSN, $user, $pass) or die "Conectare esuata: $DBI::errstr\n" unless $dbh;
			$sth = $dbh->prepare("insert into pagini_html values (NULL, '$IP', '$title');");
			$sth->execute();
			$sth->finish();

			exit;
		}
	}
}




