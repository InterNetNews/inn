##  $Id$
##
##  This is a sample filter for the Perl nnrpd hook.
##
##  See the INN Perl Filtering and Authentication Hooks documentation
##  for more information.
##
##  This file is loaded when nnrpd starts up.  If it defines a sub named
##  "filter_post", then that function will be called during processing of a
##  posting.  It has access to the headers of the article via the associative
##  array %hdr, and to useful information about the connection via the
##  associative array %attributes.
##  If it returns a null string, then the article is accepted for posting.
##  A non-null string rejects it, and the value returned is used in the
##  rejection message (make sure that such a message is properly encoded
##  in UTF-8 so as to comply with the NNTP protocol).
##
##  When filtering is disabled, the filter_end() Perl routine is called,
##  if defined, prior to the deactivation of the filter.

#
# Do any initialization steps.
#
my %config = (checkincludedtext => 0,
              includedcutoff => 40,
              includedratio => 0.6,
              quotere => '^[>:]',
              antiquotere => '^[<]',  # so as not to reject dict(1) output
             );


#
# Sample filter
#
sub filter_post {
    my $rval = "" ;             # assume we'll accept.

### Uncomment this next block to reject articles that have 'make money'
### in their subject, or which have a "Re: " subject, but no References:
### header, or which have an invalid From.
##
##    if ($hdr{"Subject"} =~ /make.*money/i) {
##        $rval = "Spam is not acceptable here..." ;
##    } elsif ($hdr{'Subject'} =~ /^Re: /o and $hdr{'References'} eq "") {
##        $rval = "Followup without References:";
##    } elsif ($hdr{'From'} =~ /^\w*$/o or
##             $hdr{'From'} !~ /^(.+?)\@([-\w\d]+\.)*([-\w\d]+)\.([-\w\d]{2,})$/o) {
##        $rval = "From: is invalid, must be user\@[host.]domain.tld";
##    }

### Uncomment this next block to reject articles that are sent from
### a network outside 10.42.0.0/16.
##
##    if ($attributes{'ipaddress'} !~ /^10\.42\./) {
##        $rval = "Unauthorized network.";
##    }

### The next block rejects articles with too much quoted text, if the
### config hash directs it to.

    if ($config{checkincludedtext}) {
        my ($lines, $quoted, $antiquoted) = analyze($body);
        if ($lines > $config{includedcutoff}
                && $quoted - $antiquoted > $lines * $config{includedratio}) {
            $rval = "Article contains too much quoted text";
        }
    }

    return $rval;
}

sub analyze {
    my ($lines, $quoted, $antiquoted) = (0, 0, 0);
    local $_ = shift;

    do {
        if ( /\G$config{quotere}/mgc ) {
            $quoted++;
        } elsif ( /\G$config{antiquotere}/mgc ) {
            $antiquoted++;
        }
    } while ( /\G(.*)\n/gc && ++$lines );

    return ($lines, $quoted, $antiquoted);
}

sub filter_end {
# Do whatever you want to clean up things when Perl filtering is disabled.
}

