#!/usr/bin/perl -w

use strict;
use DBI;

my $DSN = "dbi:mysql:dbname=poesis";

my $user = "razvan";
my $pass = "password";
my $dbh;
my $sth;

@ARGV = ("ip-index-html");

my ($IP, $title, $status);

my @linkuri;	# tabloul cu linkurile gasite

my $bigstring = "";
my $begintitle;

#my @lines = <>;

my $i = 0;

while(<>){
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
	print "\tIP = $IP";
	print "\tTitle = $title\n";
			
	# insert the obtained info into the database;
	$dbh = DBI->connect($DSN, $user, $pass) or die "Conectare esuata: $DBI::errstr\n" unless $dbh;
			
	$sth = $dbh->prepare("START TRANSACTION");
	$sth->execute();

	$sth = $dbh->prepare("INSERT INTO pagini_html VALUES (NULL, '$IP', NULL, '$title', 1);");
	$sth->execute();
	$sth = $dbh->prepare("SELECT \@idp:=LAST_INSERT_ID()");
	$sth->execute();
}

# extract any existing links
if(defined($title)){
	@ARGV = ("ip-index-html");
	while(<>){
		if(/href=(["']{1})\s*(http(s?):\/\/[^"'>]+\.(s?)html)\s*\1/i){
			if(/<\/a>/i){
				my @items = split /(<\/a>)|(<\/A>)/;
				foreach my $i (@items){
					if(defined($i)){
						if($i =~ /href=(["']{1})\s*(http(s?):\/\/[^"'>]+\.(s?)html)\s*\1/i){
							my $hiperlink = $2;
							$hiperlink =~ s/"/\\\"/g;
							$hiperlink =~ s/'/\\\'/g;
							push @linkuri, $hiperlink;
							print "\t$hiperlink\n";
						}
					}
				}	
			}else{
				#if(/href=(["']{1})\s*(http(s?):\/\/[^"'>]+\.(s?)html)\s*\1/i){
					my $hiperlink = $2;
					$hiperlink =~ s/"/\\\"/g;
					$hiperlink =~ s/'/\\\'/g;
					push @linkuri, $hiperlink;
					print "\t$hiperlink\n";
				#}
			}
		}
	}
	foreach my $legatura (@linkuri){
		$sth = $dbh->prepare("INSERT INTO referinte_html VALUES (NULL, \@idp, '$legatura');");
		$sth->execute();
	}
	$sth = $dbh->prepare("COMMIT");
	$sth->execute();
	$sth->finish();
	$dbh->disconnect;
}

