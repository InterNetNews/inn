#define	ACTIVED_TIMEOUT	5

#define	REQ_AYT		1
#define	REQ_AYTACK	2
#define	REQ_FIND	3
#define	REQ_FINDRESP	4

struct wireprotocol {
    int		RequestType;
    int		RequestID;
    int		Success;
    int		NameNull;
    char	Name[1024];
    ARTNUM	High;
    ARTNUM	Low;
    char	Flag;
    int		AliasNull;
    char	Alias[1024];
};
