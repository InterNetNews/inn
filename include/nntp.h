/*  $Id$
**
**  Here be a set of NNTP response codes as defined in RFC3977 and elsewhere.
**  The reponse codes are three digits, RFI, defined like this:
**	R, Response:
**		1xx	Informative message
**		2xx	Command ok
**		3xx	Command ok so far, send the rest of it.
**		4xx	Command was correct, but couldn't be performed for
**			some reason.
**		5xx	Command unimplemented, or incorrect, or a serious
**			program error occurred.
**	F, Function:
**		x0x	Connection, setup, and miscellaneous messages
**		x1x	Newsgroup selection
**		x2x	Article selection
**		x3x	Distribution functions
**		x4x	Posting
**		x8x	Nonstandard extensions (AUTHINFO, XGTITLE)
**		x9x	Debugging output
**	I, Information:
**		No defined semantics
*/

#include "inn/nntp.h"

#define NNTP_BAD_COMMAND		"500 Syntax error or bad command"
#define NNTP_ACCESS			"502 Permission denied"
#define NNTP_LIST_FOLLOWS		"215"
#define NNTP_ARTICLE_FOLLOWS		"220"
#define NNTP_REJECTIT_EMPTY		"437 Empty article"
#define NNTP_POSTEDOK			"240 Article posted"
#define NNTP_BAD_SUBCMD			"501 Bad subcommand"
#define NNTP_NOTINGROUP			"412 Not in a newsgroup"
#define NNTP_NOSUCHGROUP		"411 No such group"
#define NNTP_NEWNEWSOK			"230 New news follows"
#define NNTP_NOARTINGRP			"423 Bad article number"
#define NNTP_NOCURRART			"420 No current article"

/*
**  The first character of an NNTP reply can be used as a category class.
*/
#define NNTP_CLASS_OK			'2'
#define NNTP_CLASS_ERROR		'4'
#define NNTP_CLASS_FATAL		'5'


/*
**  Authentication commands from the RFC update (not official).
*/
#define NNTP_AUTH_NEEDED		"480"
#define NNTP_AUTH_BAD			"481"
#define NNTP_AUTH_NEXT			"381"
#define NNTP_AUTH_OK			"281"

/*
**  MODE CANCEL extension.
*/
#define NNTP_OK_CANCELLED       "289"

/*
**  XBATCH feed extension.
*/
#define NNTP_OK_XBATCHED	"239"
#define NNTP_CONT_XBATCH_STR	"339"
/* and one more meaning for the 436 code NNTP_FAIL_IHAVE_DEFER */
#define NNTP_RESENDIT_XBATCHERR	"436 xbatch failed: "
/* and one more meaning for the 501 code NNTP_SYNTAX_USE */
#define NNTP_XBATCH_BADSIZE	"501 Invalid or missing size for xbatch"
