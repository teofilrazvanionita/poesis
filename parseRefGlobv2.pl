#!/usr/bin/perl -w

use strict;
#use DBI;

use local::lib;

package ContentParser;
use base "HTML::Parser";

#use Fcntl qw(:seek); 

#my $DSN = "dbi:mysql:dbname=poesis:host=floare-de-colt";

#my $user = "razvan";
#my $pass = "password";
#my $dbh;
#my $sth;


my ($IP, $URL, $status);

my %linkuri;	# Linkurile extrase - in chei

my $datastring;
my $description;

my $meta_contents;
my $h1 =    "";
my $title = "";

my $h1_flag    = 0;
my $title_flag = 0;

my $i = 0;
my $html_start = 0;

sub start{
        my ($self, $tag, $attr, $attrseq, $origtext) = @_;
        
        my $href_value;

        if ($tag =~ /^meta$/i && $attr->{'name'} =~ /^description$/i) {
                $meta_contents = $attr->{'content'};
        } elsif ($tag =~ /^h1$/i && ! $h1) {
                $h1_flag = 1;
        } elsif ($tag =~ /^title$/i && ! $title) {
                $title_flag = 1;
        } elsif ($tag =~ /^a$/i) {
                if (defined $attr->{'href'}) {
                        $href_value = $attr->{'href'};
                        if($href_value =~ /http(s?):\/\/[^\/]{1}.*/i){
                                $linkuri{$href_value}++;
                        }
                }
        }
}

sub text{
        my ($self, $text) = @_;
        
        if ($h1_flag)    { $h1    .= $text; return; }
        if ($title_flag) { $title .= $text; return; }
        $datastring .= $text;
	$datastring =~ s/\s+/ /g;
}

sub end{
        my ($self, $tag, $origtext) = @_;

        if ($tag =~ /^h1$/i)    { $h1_flag = 0; }
        if ($tag =~ /^title$/i) { $title_flag = 0; }
}

open RESULT,  "<ref-glob-page" or die "Connot open ref-glob-page file: $!";

my $p = new ContentParser;

while(<RESULT>){
	if($i == 0){
		$IP = $_;
	}elsif($i == 1){
		$URL = $_;
        }elsif($i == 2){
                $status = $_;
	}elsif($status =~ /200 OK/ && !$html_start){
		if(/^\s*$/){
			$html_start = 1;
		}
	}elsif($status =~ /200 OK/ &&  $html_start){
		$p->parse($_);
	}
	$i++;
}

$p->eof;

print "THREAD 2: Summary information: ", $meta_contents || $h1 || $title ||  "No summary information found.", "\n";
foreach my $legatura (keys %linkuri){
	print "THREAD 2: $legatura\n";
}
#print "Data: $datastring\n";

close RESULT or die "Error closing filehandle: $!";

exit 0;
