#!/usr/bin/perl

##  =()<require "@<_PATH_PERL_SHELLVARS>@";>()=
require "/var/news/etc/innshellvars.pl";

if (not defined $inn::top) {
    $top = 20;
} else {
    $top = $inn::top;
}

$unwanted_log = $inn::most_logs."/OLD/unwanted.log";
$unwanted_new = $inn::most_logs."/unwanted.log";

sub unwanted_site_sort {
    $unwanted_sites{$b} <=> $unwanted_sites{$a};
};

sub junked_site_sort {
    $junked_sites{$b} <=> $junked_sites{$a};
};

sub unwanted_dists_sort {
    $unwanted_dists{$b} <=> $unwanted_dists{$a};
};

sub unwanted_newsgroup_sort {
    $unwanted_newsgroup{$b} <=> $unwanted_newsgroup{$a};
};

sub unapproved_for_sort {
    $unapproved_for{$b} <=> $unapproved_for{$a};
};

sub general_problems_sort {
    $general_problems{$b} <=> $general_problems{$a};
};

sub bad_header_sites_sort {
    $bad_header_sites{$b} <=> $bad_header_sites{$a};
};

sub aliased_sites_sort {
    $aliased_sites{$b} <=> $aliased_sites{$a};
};

$format = 0;

while (<>) {
    chop;

    if ($foramt == 0) {
        ($month, $day, $time, $stat, $site, $message_id, $code, $unwanted, $reason, $param1, $param2) = split(/ /);
        if ($day eq "") {
            ($month, $null, $day, $time, $stat, $site, $message_id, $code, $unwanted, $reason, $param1, $param2) = split(/ /);
        }
        $format = 1;
    } else {
        ($month, $null, $day, $time, $stat, $site, $message_id, $code, $unwanted, $reason, $param1, $param2) = split(/ /);
    } 
 
    if ($stat eq "+") {
	next;
    }
    
    if ($stat eq "-") {
	$unwanted_sites{$site} ++;
    }
    
    if ($stat eq "j") {
	$junked_sites{$site} ++;
    }
    
    if (($unwanted eq "Duplicate") && ($reason eq "article")) {
        next;
    }

    if (($code == "437") && ($unwanted eq "Unwanted")) {
	if ($reason eq "newsgroup") {
	    $param1 =~ s/"//g;
	    $unwanted_newsgroup{$param1}++;
	    next;
	}

	if ($reason eq "distribution") {
	    $param1 =~ s/"//g;
	    $unwanted_dists{$param1}++;
	    next;
	}

	if ($reason eq "site") {
	    $aliased_sites{$param1}++;
	    next;
	}
    }
    
    if (($code == "437") && ($unwanted eq "Unapproved") && 
	($reason eq "for")) {
	$param1 =~ s/"//g;
	$unapproved_for{$param1}++;
	next;
    }

    if (($unwanted eq "Whitespace") && ($reason eq "in")) {
        next;
    }

    if ($code == "437") {
        s/.*437 //;
        s/Too old.*/Too old/;
        s/Linecount/Linecount wrong/;
        $general_problems{$_}++;
        $bad_header_sites{$site}++;
    }
}

if (keys %unwanted_sites > 0) {
    printf "Top %s sites sending bad articles:\n", $top;
    $i = 1;
    foreach $site (sort unwanted_site_sort keys(%unwanted_sites)) {
	printf "%s %s\n", $unwanted_sites{$site}, $site;
        $i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

if (keys %junked_sites > 0) {
    printf "Top %s sites sending junked \"unwanted\" articles:\n", $top;
    $i = 1;
    foreach $site (sort junked_site_sort keys(%junked_sites)) {
	printf "%s %s\n", $junked_sites{$site}, $site;
	$i++;
	if ($i > 20) {
	    last;
	}
    }
    printf "\n";
}

if (keys %unwanted_dists > 0) {
    printf "Top %s unwanted distributions by number of articles:\n", $top;
    $i = 1;
    foreach $site (sort unwanted_dists_sort keys(%unwanted_dists)) {
	printf "%s %s\n", $unwanted_dists{$site}, $site;
        $i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

if (keys %unapproved_for > 0) {
    printf "Top %s supposedly-moderated groups with unmoderated postings:\n",
           $top;
    $i = 1;
    foreach $site (sort unapproved_for_sort keys(%unapproved_for)) {
	printf "%s %s\n", $unapproved_for{$site}, $site;
	$i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

if (keys %unwanted_newsgroup > 0) {
    printf "Top %s unwanted newsgroups by number of articles:\n", $top;
    $i = 1;
    foreach $site (sort unwanted_newsgroup_sort keys(%unwanted_newsgroup)) {
	printf "%s %s\n", $unwanted_newsgroup{$site}, $site;
	$i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

if (keys %aliased_sites > 0) {
    printf "Top %s sites that are aliased out:\n", $top;
    $i = 1;
    foreach $site (sort aliased_sites_sort keys(%aliased_sites)) {
	printf "%s %s\n", $aliased_sites{$site}, $site;
	$i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

if (keys %general_problems > 0) {
    printf "Top %s general message problems:\n", $top;
    $i = 1;
    foreach $site (sort general_problems_sort keys(%general_problems)) {
	printf "%s %s\n", $general_problems{$site}, $site;
	$i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

if (keys %bad_header_sites > 0) {
    printf "Top %s sites sending news with bad headers:\n", $top;
    $i = 1;
    foreach $site (sort bad_header_sites_sort keys(%bad_header_sites)) {
	printf "%s %s\n", $bad_header_sites{$site}, $site;
	$i++;
	if ($i > $top) {
	    last;
	}
    }
    printf "\n";
}

open(UNWANTED, $unwanted_log);

while (<UNWANTED>) {
    chop;
    ($count, $group) = split(/ /);
    $unwanted_newsgroup{$group} += $count;
}

close(UNWANTED);

unlink($unwanted_new);

open(UNWANTED, "> ".$unwanted_new); 
chmod(0660, $unwanted_new);

foreach $site (sort unwanted_newsgroup_sort keys(%unwanted_newsgroup)) {
    printf UNWANTED "%s %s\n", $unwanted_newsgroup{$site}, $site;
}

close(UNWANTED);
