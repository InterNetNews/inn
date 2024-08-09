##  Implementation of the binary protocol used to communicate with the
##  ovsqlite-server daemon.
##  Original implementation written by Bo Lindbergh in December 2023.

package INN::ovsqlite_client;

use strict;
use warnings;

our ($VERSION);

BEGIN {
    $VERSION = 1.001;
    # The integer part of the above will be used
    # as the protocol version in the server handshake.
}

use Carp qw(croak);
use Socket;

use constant HAVE_PF_UNIX => defined(&PF_UNIX);

my (@errors, @fatals);

sub add_enums {
    my $constants = shift(@_);
    my ($val);

    $val = 0;
    foreach my $name (@_) {
        if ($name =~ /^[_A-Za-z]/) {
            $constants->{$name} = $val;
            $val++;
        } elsif ($name =~ /^0/) {
            $val = oct($name);
        } else {
            $val = 0 + $name;
        }
    }
}

sub add_flags {
    my $constants = shift(@_);
    my ($val);

    $val = 1;
    foreach my $name (@_) {
        if ($name =~ /^[_A-Za-z]/) {
            $constants->{$name} = $val;
            $val <<= 1;
        } elsif ($name =~ /^0/) {
            $val = oct($name);
        } else {
            $val = 0 + $name;
        }
    }
}

our (@EXPORT_OK, %EXPORT_TAGS);

BEGIN {
    my (%constants);
    my @search_flags = qw(
        search_flag_high
    );
    my @search_cols = qw(
        search_col_arrived search_col_expires search_col_token
        search_col_overview
    );
    my @request_codes = qw(
        request_hello request_set_cutofflow request_add_group
        request_get_groupinfo request_delete_group request_list_groups
        request_add_article request_get_artinfo request_delete_article
        request_search_group request_start_expire_group
        request_expire_group request_finish_expire
    );
    my @success_codes = qw(
        response_ok response_done response_groupinfo response_grouplist
        response_grouplist_done response_artinfo response_artlist
        response_artlist_done
    );
    my @error_codes = qw(
        response_error response_sequence_error response_sql_error
        response_corrupted response_no_group response_no_article
        response_dup_article response_old_article
    );
    my @fatal_codes = qw(
        response_fatal response_bad_request response_oversized
        response_wrong_state response_wrong_version response_failed_auth
    );

    add_flags(
        \%constants,
        0x01, @search_flags,
        0x01, @search_cols,
    );

    add_enums(
        \%constants,
        0x00, @request_codes,
        0x00, @success_codes,
        0x80, @error_codes,
        0xC0, @fatal_codes,
    );

    constant::->import(\%constants);

    @EXPORT_OK = (@search_cols, @success_codes, @error_codes, @fatal_codes);

    %EXPORT_TAGS = (
        search_cols    => \@search_cols,
        response_codes => [@success_codes, @error_codes, @fatal_codes],
        all            => \@EXPORT_OK,
    );

    @errors = (
        "Error",
        "Sequence error",
        "SQL error",
        "Database corrupted",
        "No such group",
        "No such article",
        "Duplicate article",
        "Old article",
    );

    @fatals = (
        "Fatal",
        "Bad request",
        "Oversized request",
        "Wrong state",
        "Version mismatch",
        "Not authorized",
    );
}

use Exporter qw(import);

# Some helper code to convert between named and positional arguments,
# for both input and output.

sub argparser {
    my ($inspec, $outspec) = @_;
    my (@innames, %inpos, $inref, $inreq, @defix, @defval);
    my (@outnames, %outpos, $outreq);

    $inref = "";
    $inreq = "";
    foreach my $pos (0 .. $#{$inspec}) {
        my ($arg, $name);

        $arg = $inspec->[$pos];
        $name = $arg->{name};
        defined($name)
          or croak "Invalid argument name";
        exists($inpos{$name})
          and die "Duplicate argument name: $name";
        $inpos{$name} = $pos;
        if ($arg->{reference}) {
            vec($inref, $pos, 1) = 1;
        }
        if (exists($arg->{default})) {
            push(@defix, $pos);
            push(@defval, $arg->{default});
        } elsif ($arg->{required}) {
            vec($inreq, $pos, 1) = 1;
        }
        push(@innames, $name);
    }

    $outreq = "";
    foreach my $pos (0 .. $#{$outspec}) {
        my ($arg, $name);

        $arg = $outspec->[$pos];
        $name = $arg->{name};
        defined($name)
          or croak "Invalid argument name";
        exists($outpos{$name})
          and die "Duplicate argument name: $name";
        $outpos{$name} = $pos;
        if ($arg->{required}) {
            vec($outreq, $pos, 1) = 1;
        }
        push(@outnames, $name);
    }

    return (
        sub {
            my ($missing, @missing, %unknown, @unknown);

            $missing = $inreq;
            @_[@defix] = @defval;
            for (my $nix = @innames; $nix < $#_; $nix += 2) {
                my ($name, $pos);

                $name = $_[$nix];
                $pos = $inpos{$name};
                if (defined($pos)) {
                    vec($missing, $pos, 1) = 0;
                    if (vec($inref, $pos, 1)) {
                        $_[$pos] = \$_[$nix + 1];
                    } else {
                        $_[$pos] = $_[$nix + 1];
                    }
                } else {
                    exists($outpos{$name})
                      or $unknown{$name}++
                      or push(@unknown, $name);
                }
            }
            if ($missing =~ /[^\x00]/) {
                foreach my $pos (0 .. $#innames) {
                    if (vec($missing, $pos, 1)) {
                        push(@missing, $innames[$pos]);
                    }
                }
            }
            if (@missing || @unknown) {
                croak(
                    @missing ? "Missing required arguments: @missing\n" : "")
                  . (@unknown ? "Unrecognised arguments: @unknown\n" : "");
            }
        },
        sub {
            my ($missing, @missing, %unknown, @unknown);

            $missing = $outreq;
            for (my $nix = @outnames; $nix < $#_; $nix += 2) {
                my ($name, $pos);

                $name = $_[$nix];
                $pos = $outpos{$name};
                if (defined($pos)) {
                    vec($missing, $pos, 1) = 0;
                    $_[$nix + 1] = $_[$pos];
                } else {
                    exists($inpos{$name})
                      or $unknown{$name}++
                      or push(@unknown, $name);
                }
            }
            if ($missing =~ /[^\x00]/) {
                foreach my $pos (0 .. $#outnames) {
                    if (vec($missing, $pos, 1)) {
                        push(@missing, $outnames[$pos]);
                    }
                }
            }
            if (@missing || @unknown) {
                croak(
                    @missing ? "Missing required arguments: @missing\n" : "")
                  . (@unknown ? "Unrecognised arguments: @unknown\n" : "");
            }
        },
    );
}

sub send_request {
    my ($self, $pack) = splice(@_, 0, 2);
    my ($buf, $len, $got);

    $buf = pack($pack, @_);
    $len = length($buf);
    if ($len <= 0) {
        croak "Empty request";
    } elsif ($len > 0xFFFFFFFB) {
        croak "Oversized request";
    }
    $got = syswrite($self, pack("L", $len + 4));
    defined($got)
      or croak "write: $!";
    $got == 4
      or croak "Short write?!";
    $got = syswrite($self, $buf);
    defined($got)
      or croak "write: $!";
    $got == $len
      or croak "Short write?!";
}

sub receive_response($$$) {
    my $self = $_[0];
    my ($buf, $got, $len, $code, $errmsg);

    $got = sysread($self, $buf, 4);
    defined($got)
      or croak "read: $!";
    $got == 4
      or croak "Unexpected EOF from server";
    $len = unpack("L", $buf) - 4;
    $len >= 1
      or croak "Invalid response length from server";
    $_[1] = "";

    while ($len > 0) {
        $got = sysread($self, $_[1], $len, length($_[1]));
        defined($got)
          or croak "read: $!";
        $got
          or croak "Unexpected EOF from server";
        $len -= $got;
    }

    $code = unpack("C", $_[1]);
    if ($code >= response_fatal) {
        $errmsg = $fatals[$code -response_fatal];
        if (!defined($errmsg)) {
            $errmsg = sprintf("Unrecognised fatal error 0x%.2X", $code);
        }
        close($self);
    } elsif ($code >= response_error) {
        $errmsg = $errors[$code -response_error];
        if (!defined($errmsg)) {
            $errmsg = sprintf("Unrecognised error 0x%.2X", $code);
        }
        if ($code == response_sql_error) {
            my ($status, $msg);

            ($status, $msg) = unpack("x l L/a", $_[0]);
            $errmsg .= sprintf(": (%d) %s", $status, $msg);
        }
    }
    $_[2] = $code;
    $_[3] = $errmsg;
    ();
}

BEGIN {
    my ($new_in, undef) = argparser(
        [
            {
                name     => "path",
                required => 1,
            },
            {
                name    => "mode",
                default => 0,
            },
        ],
        [],
    );

    sub new {
        my $class = shift(@_);
        my ($path, $mode);
        my (@stat, $pf, $addr, $cookie, $self, $buf);
        my ($code, $errmsg);

        $new_in->(
            $path, $mode,
            @_,
        );
        @stat = stat($path)
          or croak "$path: $!";
        if (HAVE_PF_UNIX && -S _) {
            $pf = PF_UNIX;
            $addr = pack_sockaddr_un($path);
            $cookie = "";
        } elsif (-f _ && $stat[7] == 18) {
            my ($fh, $fhbuf, $got, $port, $fhcookie);

            open($fh, "<", $path)
              or croak "$path: open: $!";
            binmode($fh);
            $got = read($fh, $fhbuf, 18);
            defined($got)
              or croak "$path: read: $!";
            $got == 18
              or croak "$path: Unexpected EOF";
            close($fh);
            ($port, $fhcookie) = unpack("n a16", $fhbuf);
            $pf = PF_INET;
            $addr = pack_sockaddr_in($port, INADDR_LOOPBACK);
        } else {
            croak "$path: Don't know how to connect to this";
        }
        socket($self, $pf, SOCK_STREAM, 0)
          or croak "socket: $!";
        connect($self, $addr)
          or croak "connect: $!";
        bless($self, $class);
        $self->send_request(
            "C L L a*",
            request_hello, int($VERSION), $mode, $cookie,
        );
        $self->receive_response($buf, $code, $errmsg);
        if ($code >= response_error) {
            croak $errmsg;
        }
        $self;
    }

    my ($set_cutofflow_in, $set_cutofflow_out) = argparser(
        [
            {
                name    => "cutofflow",
                default => 0,
            }
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub set_cutofflow {
        my $self = shift(@_);
        my ($cutofflow);
        my ($buf);
        my ($code, $errmsg);

        $set_cutofflow_in->(
            $cutofflow,
            @_,
        );
        $self->send_request(
            "C C",
            request_set_cutofflow, $cutofflow,
        );
        $self->receive_response($buf, $code, $errmsg);
        $set_cutofflow_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($add_group_in, $add_group_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "flag_alias",
                required => 1,
            },
            {
                name     => "low",
                required => 1,
            },
            {
                name     => "high",
                required => 1,
            },
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub add_group {
        my $self = shift(@_);
        my ($groupname, $flag_alias, $low, $high);
        my ($namelen, $flaglen, $buf);
        my ($code, $errmsg);

        $add_group_in->(
            $groupname, $flag_alias, $low, $high,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        $flaglen = length($flag_alias);
        $flaglen > 0 && $flaglen < 0x10000
          or croak "Bad flag_alias length";
        $self->send_request(
            "C S/a Q Q S/a",
            request_add_group, $groupname, $low, $high, $flag_alias,
        );
        $self->receive_response($buf, $code, $errmsg);
        $add_group_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($get_groupinfo_in, $get_groupinfo_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            }
        ],
        [
            {
                name => "errmsg",
            },
            {
                name => "low",
            },
            {
                name => "high",
            },
            {
                name => "count",
            },
            {
                name => "flag_alias",
            },
        ],
    );

    sub get_groupinfo {
        my $self = shift(@_);
        my ($groupname);
        my ($namelen, $buf);
        my ($code, $errmsg, $low, $high, $count, $flag_alias);

        $get_groupinfo_in->(
            $groupname,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        $self->send_request(
            "C S/a",
            request_get_groupinfo, $groupname,
        );
        $self->receive_response($buf, $code, $errmsg);
        if ($code == response_groupinfo) {
            ($low, $high, $count, $flag_alias) = unpack("x Q Q Q S/a", $buf);
        }
        $get_groupinfo_out->(
            $errmsg, $low, $high, $count, $flag_alias,
            @_,
        );
        $code;
    }

    my ($delete_group_in, $delete_group_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            }
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub delete_group {
        my $self = shift(@_);
        my ($groupname);
        my ($namelen, $buf);
        my ($code, $errmsg);

        $delete_group_in->(
            $groupname,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length\n";
        $self->send_request(
            "C S/a",
            request_delete_group, $groupname,
        );
        $self->receive_response($buf, $code, $errmsg);
        $delete_group_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($list_groups_in, $list_groups_out) = argparser(
        [
            {
                name    => "groupid",
                default => 0,
            },
            {
                name    => "readsize",
                default => 0x20000,
            },
        ],
        [
            {
                name => "errmsg",
            },
            {
                name => "groupid",
            },
            {
                name     => "groups",
                required => 1,
            },
        ],
    );

    sub list_groups {
        my $self = shift(@_);
        my ($groupid, $readsize);
        my ($buf);
        my ($code, $errmsg, $groups);

        $list_groups_in->(
            $groupid, $readsize,
            @_,
        );
        $self->send_request(
            "C L q",
            request_list_groups, $readsize, $groupid,
        );
        $self->receive_response($buf, $code, $errmsg);
        if ($code == response_grouplist || $code == response_grouplist_done) {
            my ($groupcount, @groups);

            ($groupid, $groupcount) = unpack("x q L", $buf);
            if ($groupcount > 0) {
                my (@flat);

                @flat = unpack("x x8 x4 (S/a Q Q Q S/a)*", $buf);
                @flat == $groupcount * 5
                  or croak "Malformed response";
                foreach my $groupix (0 .. $groupcount - 1) {
                    my (%group);

                    @group{qw(groupname low high count flag_alias)}
                      = @flat[$groupix * 5 .. $groupix * 5 + 4];
                    push(@groups, \%group);
                }
            }
            $groups = \@groups;
        }
        $list_groups_out->(
            $errmsg, $groupid, $groups,
            @_,
        );
        return $code;
    }

    my ($list_groups_all_in, $list_groups_all_out) = argparser(
        [
            {
                name     => "callback",
                required => 1,
            },
            {
                name    => "readsize",
                default => 0x20000,
            },
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub list_groups_all {
        my $self = shift(@_);
        my ($callback, $readsize);
        my ($code, $errmsg, $groupid, $groups, $keep_on, $died);

        $list_groups_all_in->(
            $callback, $readsize,
            @_,
        );
        $groupid = 0;
        $keep_on = 1;
        while ($keep_on) {
            $code = $self->list_groups(
                groupid  => $groupid,
                readsize => $readsize,
                groups   => $groups,
                errmsg   => $errmsg,
            );
            $code == response_grouplist || $code == response_grouplist_done
              or last;
            eval {
                $keep_on = $callback->($groups);
                1;
            } or do {
                $died = $@;
                $keep_on = 0;
            };
            $code == response_grouplist_done
              and last;
        }
        $list_groups_all_out->(
            $errmsg,
            @_,
        );
        defined($died)
          and croak $died;
        $code;
    }

    my ($add_article_in, $add_article_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "artnum",
                required => 1,
            },
            {
                name     => "token",
                required => 1,
            },
            {
                name      => "overview",
                reference => 1,
                required  => 1,
            },
            {
                name    => "arrived",
                default => 0,
            },
            {
                name    => "expires",
                default => 0,
            },
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub add_article {
        my $self = shift(@_);
        my ($groupname, $artnum, $token, $overview, $arrived, $expires);
        my ($namelen, $buf);
        my ($code, $errmsg);

        $add_article_in->(
            $groupname, $artnum, $token, $overview, $arrived, $expires,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        length($token) == 18
          or croak "Bad token length";
        $self->send_request(
            "C S/a Q q q a18 L/a",
            request_add_article,
            $groupname, $artnum, $arrived, $expires, $token, ${$overview},
        );
        $self->receive_response($buf, $code, $errmsg);
        $add_article_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($get_artinfo_in, $get_artinfo_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "artnum",
                required => 1,
            },
        ],
        [
            {
                name => "errmsg",
            },
            {
                name     => "token",
                required => 1,
            },
        ],
    );

    sub get_artinfo {
        my $self = shift(@_);
        my ($groupname, $artnum);
        my ($namelen, $buf);
        my ($code, $errmsg, $token);

        $get_artinfo_in->(
            $groupname, $artnum,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        $self->send_request(
            "C S/a Q",
            request_get_artinfo, $groupname, $artnum,
        );
        $self->receive_response($buf, $code, $errmsg);
        if ($code == response_artinfo) {
            $token = unpack("x a18", $buf);
            length($token) == 18
              or croak "Malformed response";
        }
        $get_artinfo_out->(
            $errmsg, $token,
            @_,
        );
        $code;
    }

    my ($delete_article_in, $delete_article_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "artnum",
                required => 1,
            },
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub delete_article {
        my $self = shift(@_);
        my ($groupname, $artnum);
        my ($namelen, $buf);
        my ($code, $errmsg);

        $delete_article_in->(
            $groupname, $artnum,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        $self->send_request(
            "C S/a Q",
            request_delete_article, $groupname, $artnum,
        );
        $self->receive_response($buf, $code, $errmsg);
        $delete_article_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($search_group_in, $search_group_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "low",
                required => 1,
            },
            {
                name => "high",
            },
            {
                name    => "cols",
                default => 0,
            },
            {
                name    => "readsize",
                default => 0x20000,
            },
        ],
        [
            {
                name => "errmsg",
            },
            {
                name     => "articles",
                required => 1,
            },
        ],
    );

    sub search_group {
        my $self = shift(@_);
        my ($groupname, $low, $high, $cols, $readsize);
        my ($namelen, $buf);
        my ($code, $errmsg, $articles);

        $search_group_in->(
            $groupname, $low, $high, $cols, $readsize,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        if (defined($high)) {
            $self->send_request(
                "C L C C S/a Q Q",
                request_search_group,
                $readsize,
                search_flag_high,
                $cols,
                $groupname,
                $low,
                $high,
            );
        } else {
            $self->send_request(
                "C L C C S/a Q",
                request_search_group,
                $readsize,
                0x00,
                $cols,
                $groupname,
                $low,
            );
        }
        $self->receive_response($buf, $code, $errmsg);
        if ($code == response_artlist || $code == response_artlist_done) {
            my ($respcols, $artcount, @articles);

            ($respcols, $artcount) = unpack("x C L", $buf);
            if ($artcount > 0) {
                my ($unpack, @keys, @flat);

                $unpack = "Q";
                push(@keys, "artnum");
                if ($respcols & search_col_arrived) {
                    $unpack .= " q";
                    push(@keys, "arrived");
                }
                if ($respcols & search_col_expires) {
                    $unpack .= " q";
                    push(@keys, "expires");
                }
                if ($respcols & search_col_token) {
                    $unpack .= " a18";
                    push(@keys, "token");
                }
                if ($respcols & search_col_overview) {
                    $unpack .= " L/a";
                    push(@keys, "overview");
                }

                @flat = unpack("x x x4 ($unpack)*", $buf);
                @flat == $artcount * @keys
                  or croak "Malformed response";
                foreach my $artix (0 .. $artcount - 1) {
                    my (%article);

                    @article{@keys}
                      = @flat[$artix * @keys .. ($artix + 1) * @keys - 1];
                    push(@articles, \%article);
                }
            }
            $articles = \@articles;
        }
        $search_group_out->(
            $errmsg, $articles,
            @_,
        );
        return $code;
    }

    my ($search_group_all_in, $search_group_all_out) = argparser(
        [
            {
                name     => "callback",
                required => 1,
            },
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "low",
                required => 1,
            },
            {
                name => "high",
            },
            {
                name    => "cols",
                default => 0,
            },
            {
                name    => "readsize",
                default => 0x20000,
            },
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub search_group_all {
        my $self = shift(@_);
        my ($callback, $groupname, $low, $high, $cols, $readsize);
        my ($articles, $code, $errmsg, $keep_on, $died);

        $search_group_all_in->(
            $callback, $groupname, $low, $high, $cols, $readsize,
            @_,
        );
        $keep_on = 1;
        while ($keep_on) {
            $code = $self->search_group(
                groupname => $groupname,
                low       => $low,
                high      => $high,
                cols      => $cols,
                readsize  => $readsize,
                articles  => $articles,
                errmsg    => $errmsg,
            );
            $code == response_artlist || $code == response_artlist_done
              or last;
            eval {
                $keep_on = $callback->($articles);
                1;
            } or do {
                $died = $@;
                $keep_on = 0;
            };
            $code == response_artlist_done
              and last;
            $low = $articles->[-1]->{artnum} + 1;
        }
        $search_group_all_out->(
            $errmsg,
            @_,
        );
        defined($died)
          and croak $died;
        $code;
    }

    my ($start_expire_group_in, $start_expire_group_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            }
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub start_expire_group {
        my $self = shift(@_);
        my ($groupname);
        my ($namelen, $buf);
        my ($code, $errmsg);

        $start_expire_group_in->(
            $groupname,
            @_,
        );
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        $self->send_request(
            "C S/a",
            request_start_expire_group, $groupname,
        );
        $self->receive_response($buf, $code, $errmsg);
        $start_expire_group_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($expire_group_in, $expire_group_out) = argparser(
        [
            {
                name     => "groupname",
                required => 1,
            },
            {
                name     => "artnums",
                required => 1,
            },
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub expire_group {
        my $self = shift(@_);
        my ($groupname, $artnums);
        my ($namelen, $buf);
        my ($code, $errmsg);

        $expire_group_in->(
            $groupname, $artnums,
            @_,
        );
        eval {
            exists($artnums->[0]);
            1;
        } or croak $@;
        $namelen = length($groupname);
        $namelen > 0 && $namelen < 0x10000
          or croak "Bad group name length";
        $self->send_request(
            "C S/a L/Q",
            request_expire_group, $groupname, @{$artnums},
        );
        $self->receive_response($buf, $code, $errmsg);
        $expire_group_out->(
            $errmsg,
            @_,
        );
        $code;
    }

    my ($finish_expire_in, $finish_expire_out) = argparser(
        [],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub finish_expire {
        my $self = shift(@_);
        my ($buf);
        my ($code, $errmsg);

        $finish_expire_in->(@_);
        $self->send_request(
            "C",
            request_finish_expire,
        );
        $self->receive_response($buf, $code, $errmsg);
        $finish_expire_out->(
            $errmsg,
            @_,
        );
        return $code;
    }

    my ($finish_expire_all_in, $finish_expire_all_out) = argparser(
        [
            {
                name     => "callback",
                required => 1,
            }
        ],
        [
            {
                name => "errmsg",
            }
        ],
    );

    sub finish_expire_all {
        my $self = shift(@_);
        my ($callback);
        my ($code, $errmsg, $keep_on, $died);

        $finish_expire_all_in->(
            $callback,
            @_,
        );
        $keep_on = 1;
        while ($keep_on) {
            $code = $self->finish_expire(
                errmsg => $errmsg,
            );
            $code == response_ok || $code == response_done
              or last;
            eval {
                $keep_on = $callback->();
                1;
            } or do {
                $died = $@;
                $keep_on = 0;
            };
            $code == response_done
              and last;
        }
        $finish_expire_all_out->(
            $errmsg,
            @_,
        );
        defined($died)
          and croak $died;
        $code;
    }

}

1;

__END__

=head1 NAME

INN::ovsqlite_client - Talk to ovsqlite-server from Perl

=head1 SYNOPSIS

    use INN::ovsqlite_client qw(:all);

    my $client
      = INN::ovsqlite_client::->new(
          port => "/usr/local/news/run/ovsqlite.sock");

    $client->search_group_all(
        groupname => "news.software.nntp",
        low       => 1,
        cols      => search_col_overview,
        errmsg    => my $errmsg,
        callback  => sub {
            my ($articles) = @_;

            foreach my $article (@{$articles}) {
                print $article->{overview};
            }
            1;
        },
    );

    defined($errmsg)
      and die "search_group_all: $errmsg";

=head1 DESCRIPTION

C<INN::ovsqlite_client> implements the binary protocol used to communicate
with the B<ovsqlite-server> daemon.  It offers one instance method for each
request type plus convenience methods for those requests that need to be
repeated.  See F<ovsqlite-private.h> for details.

Two examples of use within a Perl script are present in the F<contrib>
directory (B<ovsqlite-dump> and B<ovsqlite-undump>).

=head1 EXPORTS

=over 4

=item tag :search_cols

Flags to select what optional information the C<search_group> method should
return.

=over 8

=item search_col_arrived

=item search_col_expires

=item search_col_token

=item search_col_overview

=back

Z<>

=item tag :response_codes

Success responses:

=over 8

=item response_ok

=item response_done

=item response_groupinfo

=item response_grouplist

=item response_grouplist_done

=item response_artinfo

=item response_artlist

=item response_artlist_done

=back

Z<>

Error responses:

=over 8

=item response_error

=item response_sequence_error

=item response_sql_error

=item response_corrupted

=item response_no_group

=item response_no_article

=item response_dup_article

=item response_old_article

=back

Fatal error responses:

=over 8

=item response_fatal

=item response_bad_request

=item response_oversized

=item response_wrong_state

=item response_wrong_version

=item response_failed_auth

=back

Z<>

=item tag :all

All of the above.

=back

=head1 CONSTRUCTOR

    $client = INN::ovsqlite_client::->new(
        path => $path,
        mode => $mode,         # optional, default 0 (read only)
    );

Croaks on failure.

=head1 METHODS

All request methods return the response code and store any error message in
the optional C<errmsg> named argument.

Communication or protocol errors (as opposed to server errors) cause croaks.

=over 4

=item set_cutofflow

    $code = $client->set_cutofflow(
        cutofflow  => $cutofflow,     # optional, default 0 (false)
        errmsg     => $errmsg,        # optional output
    );

=item add_group

    $code = $client->add_group(
        groupname  => $groupname,
        low        => $low,
        high       => $high,
        flag_alias => $flag_alias,
        errmsg     => $errmsg,        # optional output
    );

=item get_groupinfo

    $code = $client->get_groupinfo(
        groupname  => $groupname,
        low        => $low,           # optional output
        high       => $high,          # optional output
        count      => $count,         # optional output
        flag_alias => $flag_alias,    # optional output
        errmsg     => $errmsg,        # optional output
    );

=item delete_group

    $code = $client->delete_group(
        groupname  => $groupname,
        errmsg     => $errmsg,        # optional output
    );

=item list_groups

    $code = $client->list_groups(
        groupid    => $groupid,       # optional in/output, default 0
        readsize   => $readsize,      # optional, default 0x20000
        groups     => $groups,        # output
        errmsg     => $errmsg,        # optional output
    );

C<$groups> is set to a reference to an array of references to hashes with
these keys:

=over 8

=item groupname

=item low

=item high

=item count

=item flag_alias

=back

Z<>

=item list_groups_all

    $code = $client->list_groups_all(
        callback   => \&callback,
        readsize   => $readsize,      # optional, default 0x20000
        errmsg     => $errmsg,        # optional output
    );

This convenience method calls C<list_groups> repeatedly to fetch information
for all groups.  The callback function is called with the I<groups> array
reference as the only argument.  It should return a true value to keep
iterating or a false value to terminate.

=item add_article

    $code = $client->add_artice(
        groupname  => $groupname,
        artnum     => $artnum,
        token      => $token,
        arrived    => $arrived,       # optional, default 0
        expires    => $expires,       # optional, default 0
        overview   => $overview,
        errmsg     => $errmsg,        # optional output
    );

=item get_artinfo

    $code = $client->get_artinfo(
        groupname  => $groupname,
        artnum     => $artnum,
        token      => $token,         # output
        errmsg     => $errmsg,        # optional output
    );

=item delete_article

    $code = $client->delete_article(
        groupname  => $groupname,
        artnum     => $artnum,
        errmsg     => $errmsg,        # optional output
    );

=item search_group

    $code = $client->search_group(
        groupname  => $groupname,
        low        => $low,
        high       => $high,          # optional
        cols       => $cols,          # optional, default 0x00
        readsize   => $readsize,      # optional, default 0x20000
        articles   => $articles,      # output
        errmsg     => $errmsg,        # optional output
    );

C<$articles> is set to a reference to an array of references to hashes with
these keys:

=over 8

=item artnum

=item arrived

=item expires

=item token

=item overview

=back

Z<>

=item search_group_all

    $code = $client->search_group_all(
        groupname  => $groupname,
        low        => $low,
        high       => $high,          # optional
        cols       => $cols,          # optional, default 0x00
        callback   => \&callback,
        readsize   => $readsize,      # optional, default 0x20000
        errmsg     => $errmsg,        # optional output
    );

This convenience methods calls C<search_group> repeatedly to fetch information
for all specified articles.  The callback function is called with the
I<articles> array reference as the only argument.  It should return a true
value to keep iterating or a false value to terminate.

=item start_expire_group

    $code = $client->start_expire_group(
        groupname  => $groupname,
        errmsg     => $errmsg,        # optional output
    );

=item expire_group

    $code = $client->expire_group(
        groupname  => $groupname,
        artnums    => \@artnums,
        errmsg     => $errmsg,        # optional output
    );

C<@artnums> must be an array of article numbers.

=item finish_expire

    $code = $client->finish_expire(
        errmsg     => $errmsg,        # optional output
    );

=item finish_expire_all

    $code = $client->finish_expire_all(
        callback   => \&callback,
        errmsg     => $errmsg,        # optional output
    );

This convenience method calls C<finish_expire> repeatedly until done.
The callback function is called with no arguments and should return a true
value to keep iterating or a false value to terminate.

=back

=head1 HISTORY

Initial implementation and documentation written by Bo Lindbergh for
InterNetNews.

=head1 SEE ALSO

perl(1), ovsqlite-server(8).

=cut
