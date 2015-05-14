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

my $bigstring;

my @lines = <>;

if($#lines >= 2){
	$IP = $lines[0];
	$status = $lines[1];
	if($status =~ /200 OK/){	# if access was permited and some page returned
		foreach (@lines){
			chomp;
			$bigstring .= $_;	# put everything into a big scalar variable
		}
		if($bigstring =~ /<title>(.+)<\/title>/i){	# if there is a TITLE, extract it
			if($1 =~ /<title>|<\/title>/i){		# if there are nested title tags inside, exclude this
				$title = undef;
			}else{
				$title = $1;
				$title =~ s/"/\\\"/g;		# put a backslash in front of " and '
				$title =~ s/'/\\\'/g;
			}
		}
		if(defined($title)){
			print "\tIP = $IP";
			print "\tTitle = $title\n";
			
			# insert the obtained info into the database;
			$dbh = DBI->connect($DSN, $user, $pass) or die "Conectare esuata: $DBI::errstr\n" unless $dbh;
			
			$sth = $dbh->prepare("START TRANSACTION");
			$sth->execute();

			$sth = $dbh->prepare("INSERT INTO pagini_html VALUES (NULL, '$IP', '$title');");
			$sth->execute();
			$sth = $dbh->prepare("SELECT \@idp:=LAST_INSERT_ID()");
			$sth->execute();


		}
	}
}

# extract any existing links
if(defined($title)){
	@ARGV = ("ip-index-html");
	while(<>){
		if(/href=(["']{1})(http(s?):\/\/.+\.(s?)html)\1/){
			#print $_;
			my $strlength = length $_;
			my $count = 24;
			my $partstr = $_;
			while($count <= $strlength){
				$partstr = substr($_, 0, $count);
				if($partstr =~ /href=(["']{1})(http(s?):\/\/.+\.(s?)html)\1/){
					push @linkuri, $2;
					print "\t$2\n";
					$strlength = $strlength - $count;
					$_ = substr($_, $count, $strlength);
					$count = 23;
				}
				$count++;	
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

