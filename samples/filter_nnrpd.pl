#
# $Id$
#
# Sample perl filtering code for nnrpd hook.
#

#
# This file is loaded when nnrpd starts up. If it defines a sub named
# `filter_post', then that function will be called during processing of a
# posting. It has access to the headers of the article via the associative
# array `%hdr'. If it returns a null string then the article is accepted
# for posting. A non-null stringrejects it, and the value returned is used
# in the rejection message.
#

#
# Sample filter
#
sub filter_post {
    my $rval = "" ;		# assume we'll accept.

### Uncomment this next block to reject articles that have 'make money'
### in their subject, or which have a "Re: " subject, but no References:
### header, or which have an invalid From.

##    if ($hdr{"Subject"} =~ /make.*money/i) {
##	$rval = "Spam is not acceptable here..." ;
##    } elsif ($hdr{'Subject'} =~ /^Re: /o and $hdr{'References'} eq "") {
##	$rval = "Followup without References:";
##    } elsif ($hdr{'From'} =~ /^\w*$/o or
##	     $hdr{'From'} !~ /^(.+?)\@([-\w\d]+\.)*([-\w\d]+)\.([-\w\d]{2,})$/o) {
##	$rval = "From: is invalid, must be user\@[host.]domain.tld";
##    }

	$rval ;
}
