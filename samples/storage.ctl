# This file is used to determine which storage method articles are sent to
# to be stored and which storage class they are stored as.  

#
#	Sample for the ``timehash'' storage method:
#
#	methodname:wildmat:storage class #:minsize:maxsize
#
#timehash:*:0
#timehash:alt.binaries.*:1:2:32000
#timehash:alt.*:2:1

#
#	Sample for the ``cnfs'' storage method:
#
#	methodname:wildmat:storage class #:minsize:maxsize:metacycbuffname
#
#cnfs:*:1:0:3999:SMALLAREA
#cnfs:*:2:4000:1000000:BIGAREA

