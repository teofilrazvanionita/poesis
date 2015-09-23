#!/usr/bin/perl -w

use strict;
use DBI;

my $DSN = "dbi:mysql:dbname=poesis";

my $user = "razvan";
my $pass = "password";
my $dbh;
my $sth;


my ($IP, $title, $status);

my @linkuri;	# tabloul cu linkurile gasite

my @unique_links; # tabloul cu linkurile unice gasite

my $bigstring = "";
my $begintitle;

#my @lines = <>;

my $i = 0;

sub uniq {
	my %seen;
	return grep { !$seen{$_} ++ } @_;
}

open RESULT,  "<ip-index-html" or die "Connot create/open ip-index-html file: $!";

while(<RESULT>){
	if($i == 0){
		$IP = $_;
	}elsif($i == 1){
		$status = $_;
	}elsif($status =~ /200 OK/){
		if(/<title>/i && !defined($begintitle)){
			$begintitle = 1;
			if(/<\/title/i){
				($title) = /<title>([^>]+)<\/title>/i;
				if(defined($title)){
					$title =~ s/"/\\\"/g;	# put a backslash in front of "
					$title =~ s/'/\\\'/g;	# ... and '
					$title =~ s/^\s*//;	# discard heading spaces
					$title =~ s/\s*$//;	# discard trailing spaces 
				}
				last;
			}else{
				chomp;
				$bigstring .= $_;
			}
		}elsif(defined($begintitle)){
			if(/<\/title>/i){
				chomp;
				$bigstring .= $_;
				last;
			}else{
				chomp;
				$bigstring .= $_;
			}
		}
	}
	$i++;
}

if(!defined($title)){
	($title) = ($bigstring =~ /<title>(.+)<\/title>/i);
	if(defined($title)){
		$title =~ s/"/\\\"/g;
		$title =~ s/'/\\\'/g;
		$title =~ s/^\s*//;	
		$title =~ s/\s*$//;
	}
}

if(defined($title)){
	print "THREAD 1: IP = $IP";
	print "THREAD 1: Title = $title\n";
			
	# insert the obtained info into the database;
	$dbh = DBI->connect($DSN, $user, $pass) or die "Conectare esuata: $DBI::errstr\n" unless $dbh;
			
	$sth = $dbh->prepare("START TRANSACTION");
	$sth->execute();

	$sth = $dbh->prepare("INSERT INTO pagini_html VALUES (NULL, '$IP', NULL, '$title', 1);");
	$sth->execute();
	$sth = $dbh->prepare("SELECT \@idp:=LAST_INSERT_ID()");
	$sth->execute();

	
	seek(RESULT, 0, SEEK_SET);	# go back to the beginning of file

	# extract any existing links
	while(<RESULT>){
		if(m%href=(["']{1})\s*(http(s?):\/\/[^"'>/]{1}[^"'>]+\.((s?)html)|(php))\s*\1%i){
			if(/<\/a>/i){
				my @items = split /(<\/a>)|(<\/A>)/;
				foreach my $i (@items){
					if(defined($i)){
						if($i =~ m%href=(["']{1})\s*(http(s?):\/\/[^"'>/]{1}[^"'>]+\.((s?)html)|(php))\s*\1%i){
							my $hiperlink = $2;
							$hiperlink =~ s/"/\\\"/g;
							$hiperlink =~ s/'/\\\'/g;
							push @linkuri, $hiperlink;
							# print "THREAD 1: $hiperlink\n";
						}
					}
				}	
			}else{
				#if(/href=(["']{1})\s*(http(s?):\/\/[^"'>]+\.(s?)html)\s*\1/i){
					my $hiperlink = $2;
					$hiperlink =~ s/"/\\\"/g;
					$hiperlink =~ s/'/\\\'/g;
					push @linkuri, $hiperlink;
					# print "THREAD 1: $hiperlink\n";
				#}
			}
		}
	}
	
	@unique_links = &uniq(@linkuri);

	foreach my $legatura (@unique_links){
		$sth = $dbh->prepare("INSERT INTO referinte_html VALUES (NULL, \@idp, '$legatura');");
		$sth->execute();
		print "THREAD 1: $legatura\n";
	}
	$sth = $dbh->prepare("COMMIT");
	$sth->execute();
	$sth->finish();
	$dbh->disconnect;
}

close RESULT or die "Error closing filehandle: $!";

exit 0;
