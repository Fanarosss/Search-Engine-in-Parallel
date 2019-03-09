struct PListNode{
    int Doc_id;
    int File_id;
    char* DocName;
    int Amount;
    struct PListNode* Next;
};

struct PList{                                                                   //Posting List head
    int Total;                                                                  //total amount of appearances
    struct PListNode* First;
};

struct Trie{
    char Entry;
    struct Trie* SamePrefix;
    struct Trie* Next;
    struct PList* PostingList;
};

typedef struct PListNode PListNode;
typedef struct PList PList;
typedef struct Trie Trie;

void CreateTrie();
void Insert(char buffer, int document_id, int fileid, char* DocName);
int CheckValidity(char buffer);
void TrieInsert(char buffer, int document_id, int fileid, char* DocName);
Trie* GetRoot();
void df(Trie*, char*, int, FILE*);
int tf(int,char*,int);
PListNode* dfSingle(char* word, int offset);
PListNode* maxcount(char* keyword);
PListNode* mincount(char* keyword);
void TrieDelete(Trie*);
double my_clock();
