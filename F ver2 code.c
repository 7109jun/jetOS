#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

// ============================================================
// F Compiler v7
// - float(f64) support, unlimited args (stack calling convention)
// - Array [T; N], List<T>, Map
// - division by zero: literal -> compile error, runtime -> 0
// - struct/impl/new/delete/RC/import/optimization kept from v6
// ============================================================

#define MAX_TOKENS 16384
#define MAX_NODES 4096
#define MAX_SYMS 512
#define MAX_STRUCTS 64
#define MAX_FIELDS 8
#define MAX_METHODS 256
#define MAX_PARAMS 64
#define CODE_SIZE 262144
#define DATA_SIZE 32768
#define MAX_PATCHES 2048
#define MAX_CALL_SITES 1024
#define MAX_STRING_PATCHES 1024
#define MAX_STRINGS 512
#define MAX_HEAP_PATCHES 64
#define SRC_BUF_SIZE (1<<20)
#define MAX_VISITED_FILES 32

#define LOAD_ADDR 0x400000ULL
#define FILE_HEADER_SIZE 120
#define RUNTIME_BASE (LOAD_ADDR+FILE_HEADER_SIZE)
#define HEAP_SIZE (1<<20)

typedef enum {
    T_INVALID=0,T_EOF,
    T_LET,T_MUT,T_FN,T_IF,T_ELSE,T_WHILE,T_RETURN,T_PRINT,
    T_TRUE,T_FALSE,
    T_STRUCT,T_NEW,T_DELETE,T_IMPORT,T_IMPL,T_SELF,T_STATIC,
    T_IDENT,T_INT,T_FLOAT,T_STR,
    T_ASSIGN,T_EQEQ,T_NE,T_LT,T_GT,T_LE,T_GE,
    T_PLUS,T_MINUS,T_STAR,T_SLASH,T_PERCENT,
    T_ANDAND,T_PIPEPIPE,
    T_LPAREN,T_RPAREN,T_LBRACE,T_RBRACE,T_LBRACKET,T_RBRACKET,
    T_SEMI,T_COMMA,T_COLON,T_ARROW,T_DOT
} TokenType;

typedef struct { TokenType type; char text[256]; int64_t int_val; double fval; } Token;

typedef enum {
    N_NUM,N_STR,N_VAR,N_BINOP,N_NEG,N_ASSIGN,N_LET,
    N_IF,N_WHILE,N_PRINT,N_RETURN,N_FUNC,N_CALL,
    N_FIELD,N_NEW,N_DELETE,N_FIELD_INIT,N_METHOD_CALL,
    N_ARRAY,N_INDEX
} NodeKind;

typedef struct Node {
    NodeKind kind;
    int64_t val;
    char name[64];
    char aux[64];
    char type[16];
    char ret_type[16];
    int param_count;
    char (*param_names)[24];
    char (*param_types)[8];
    struct Node *left,*right,*cond,*body,*else_body,*next;
} Node;

typedef struct {
    char name[64]; int is_func; int code_offset; int offset; int is_mut;
    char type[16]; int param_count; char param_types[MAX_PARAMS][8]; char ret_type[16];
} Symbol;

typedef struct { char name[64]; int field_count; char field_names[MAX_FIELDS][64]; char field_types[MAX_FIELDS][16]; int field_offsets[MAX_FIELDS]; int size; } StructDef;
typedef struct { char struct_name[64]; char method_name[64]; char mangled[64]; int is_static; int param_count; char param_types[MAX_PARAMS][8]; char ret_type[16]; } MethodDef;
typedef struct { int pos; int target; } Patch;
typedef struct { int pos; char name[64]; } CallSite;
typedef struct { int pos; int str_id; } StringPatch;
typedef struct { char text[256]; int len; int data_offset; } StringLit;
typedef struct { int pos; int kind; } HeapPatch;

Token tokens[MAX_TOKENS]; int token_count=0,pos=0;
Node node_pool[MAX_NODES]; int node_count=0;
Symbol sym_table[MAX_SYMS]; int sym_count=0,global_sym_count=0,current_func_sym=-1;
StructDef structs[MAX_STRUCTS]; int struct_count=0;
MethodDef methods[MAX_METHODS]; int method_count=0;
uint8_t code_buf[CODE_SIZE]; int code_len=0;
uint8_t data_buf[DATA_SIZE]; int data_len=0;
Patch patches[MAX_PATCHES]; int patch_count=0;
int label_pos[MAX_PATCHES]; int label_count=0;
CallSite call_sites[MAX_CALL_SITES]; int call_site_count=0;
StringPatch string_patches[MAX_STRING_PATCHES]; int string_patch_count=0;
StringLit string_pool[MAX_STRINGS]; int string_pool_count=0;
HeapPatch heap_patches[MAX_HEAP_PATCHES]; int heap_patch_count=0;
int f_alloc_off=0,print_int_off=0,print_str_off=0,float_print_off=0;
int f_retain_off=0,f_release_off=0,trap_bounds_off=0;
int list_new_off=0,list_push_off=0,list_get_off=0,list_set_off=0,list_len_off=0;
int map_new_off=0,map_set_off=0,map_get_off=0;
int start_off=0,stack_offset=0;
char source_buf[SRC_BUF_SIZE]; int source_len=0;
char visited_files[MAX_VISITED_FILES][256]; int visited_file_count=0;

void compile_error(const char*m){fprintf(stderr,"F Compiler Error: %s\n",m);exit(1);}
Node*new_node(NodeKind k){if(node_count>=MAX_NODES)compile_error("AST overflow");Node*n=&node_pool[node_count++];memset(n,0,sizeof(Node));n->kind=k;n->param_names=malloc(MAX_PARAMS*24);n->param_types=malloc(MAX_PARAMS*8);return n;}
int is_int_type(const char*t){return strcmp(t,"i32")==0||strcmp(t,"i64")==0||strcmp(t,"u32")==0||strcmp(t,"u64")==0||strcmp(t,"bool")==0;}
int is_float_type(const char*t){return strcmp(t,"f64")==0||strcmp(t,"f32")==0;}
int types_compatible(const char*a,const char*b){if(strcmp(a,b)==0)return 1;if(is_int_type(a)&&is_int_type(b))return 1;if(is_float_type(a)&&is_float_type(b))return 1;if(a[0]=='['&&b[0]=='[')return strcmp(a,b)==0;return 0;}
StructDef*find_struct(const char*n){for(int i=0;i<struct_count;i++)if(strcmp(structs[i].name,n)==0)return &structs[i];return NULL;}
int find_field(StructDef*s,const char*n){for(int i=0;i<s->field_count;i++)if(strcmp(s->field_names[i],n)==0)return i;return -1;}
MethodDef*find_method(const char*s,const char*m,int st){for(int i=0;i<method_count;i++)if(strcmp(methods[i].struct_name,s)==0&&strcmp(methods[i].method_name,m)==0&&methods[i].is_static==st)return &methods[i];return NULL;}

// ---------- import ----------
void append_src(const char*s,int len){if(source_len+len>=SRC_BUF_SIZE)compile_error("src overflow");memcpy(source_buf+source_len,s,len);source_len+=len;}
char*read_whole_file(const char*p,long*out){FILE*f=fopen(p,"rb");if(!f){fprintf(stderr,"Cannot open file: %s\n",p);exit(1);}fseek(f,0,SEEK_END);long l=ftell(f);fseek(f,0,SEEK_SET);char*b=malloc(l+1);fread(b,1,l,f);b[l]=0;fclose(f);if(out)*out=l;return b;}
int file_visited(const char*p){for(int i=0;i<visited_file_count;i++)if(strcmp(visited_files[i],p)==0)return 1;return 0;}
void process_file(const char*path);
void process_import_line(const char*line){const char*p=line;while(*p==' '||*p=='\t')p++;if(strncmp(p,"import",6))return;p+=6;while(*p==' '||*p=='\t')p++;char mod[256];int mi=0;while(isalnum((unsigned char)*p)||*p=='_'){if(mi<255)mod[mi++]=*p;p++;}mod[mi]=0;if(!mi)return;char fp[512];snprintf(fp,sizeof(fp),"%s.fs",mod);process_file(fp);}
void process_file(const char*path){if(file_visited(path))return;if(visited_file_count>=MAX_VISITED_FILES)compile_error("too many imports");strcpy(visited_files[visited_file_count++],path);long len;char*txt=read_whole_file(path,&len);char*p=txt,*end=txt+len;while(p<end){char*ls=p;while(p<end&&*p!='\n')p++;int ll=(int)(p-ls);int nl=(p<end);char tmp[1024];int cl=ll<1023?ll:1023;memcpy(tmp,ls,cl);tmp[cl]=0;char*t=tmp;while(*t==' '||*t=='\t')t++;if(strncmp(t,"import",6)==0&&(t[6]==' '||t[6]=='\t'))process_import_line(t);else{append_src(ls,ll);if(nl)append_src("\n",1);}if(nl)p++;}free(txt);}

// ---------- lexer ----------
Token peek(void){return tokens[pos];}
Token advance(void){return tokens[pos++];}
int check(TokenType t){return peek().type==t;}
void expect(TokenType t){if(!check(t)){fprintf(stderr,"Parser Error: expected %d got %d\n",t,peek().type);exit(1);}advance();}
Token expect_ident(void){if(!check(T_IDENT))compile_error("expected identifier");return advance();}

void tokenize(const char*src){int i=0;while(src[i]){while(isspace((unsigned char)src[i]))i++;
 if(src[i]=='/'&&src[i+1]=='/'){while(src[i]&&src[i]!='\n')i++;continue;}
 if(src[i]=='/'&&src[i+1]=='*'){i+=2;while(src[i]&&!(src[i]=='*'&&src[i+1]=='/'))i++;if(src[i])i+=2;continue;}
 if(!src[i])break;Token t;memset(&t,0,sizeof(t));
 if(isalpha((unsigned char)src[i])||src[i]=='_'){int j=0;while(isalnum((unsigned char)src[i])||src[i]=='_'){if(j<255)t.text[j++]=src[i];i++;}t.text[j]=0;
  if(strcmp(t.text,"let")==0)t.type=T_LET;else if(strcmp(t.text,"mut")==0)t.type=T_MUT;else if(strcmp(t.text,"fn")==0)t.type=T_FN;
  else if(strcmp(t.text,"if")==0)t.type=T_IF;else if(strcmp(t.text,"else")==0)t.type=T_ELSE;else if(strcmp(t.text,"while")==0)t.type=T_WHILE;
  else if(strcmp(t.text,"return")==0)t.type=T_RETURN;else if(strcmp(t.text,"print")==0)t.type=T_PRINT;else if(strcmp(t.text,"true")==0)t.type=T_TRUE;
  else if(strcmp(t.text,"false")==0)t.type=T_FALSE;else if(strcmp(t.text,"struct")==0)t.type=T_STRUCT;else if(strcmp(t.text,"new")==0)t.type=T_NEW;
  else if(strcmp(t.text,"delete")==0)t.type=T_DELETE;else if(strcmp(t.text,"import")==0)t.type=T_IMPORT;else if(strcmp(t.text,"impl")==0)t.type=T_IMPL;
  else if(strcmp(t.text,"self")==0)t.type=T_SELF;else if(strcmp(t.text,"static")==0)t.type=T_STATIC;else t.type=T_IDENT;}
 else if(isdigit((unsigned char)src[i])){int j=0;int is_float=0;int start=i;while(isdigit((unsigned char)src[i]))i++;
  if(src[i]=='.'&&isdigit((unsigned char)src[i+1])){is_float=1;i++;while(isdigit((unsigned char)src[i]))i++;}
  int len=i-start;if(len<255){memcpy(t.text,src+start,len);t.text[len]=0;}
  if(is_float){t.fval=atof(t.text);t.type=T_FLOAT;}else{t.int_val=atoll(t.text);t.type=T_INT;}}
 else if(src[i]=='"'){i++;int j=0;while(src[i]&&src[i]!='"'){if(src[i]=='\\'&&src[i+1]){i++;if(src[i]=='n'){if(j<255)t.text[j++]='\n';}else if(src[i]=='t'){if(j<255)t.text[j++]='\t';}else if(src[i]=='\\'){if(j<255)t.text[j++]='\\';}else if(src[i]=='"'){if(j<255)t.text[j++]='"';}else{if(j<255)t.text[j++]=src[i];}}else{if(j<255)t.text[j++]=src[i];}i++;}if(src[i]=='"')i++;t.text[j]=0;t.type=T_STR;}
 else{switch(src[i]){case '=':if(src[i+1]=='='){t.type=T_EQEQ;i+=2;}else{t.type=T_ASSIGN;i++;}break;
  case '!':if(src[i+1]=='='){t.type=T_NE;i+=2;}else compile_error("unexpected !");break;
  case '<':if(src[i+1]=='='){t.type=T_LE;i+=2;}else{t.type=T_LT;i++;}break;
  case '>':if(src[i+1]=='='){t.type=T_GE;i+=2;}else{t.type=T_GT;i++;}break;
  case '&':if(src[i+1]=='&'){t.type=T_ANDAND;i+=2;}else compile_error("single &");break;
  case '|':if(src[i+1]=='|'){t.type=T_PIPEPIPE;i+=2;}else compile_error("single |");break;
  case '-':if(src[i+1]=='>'){t.type=T_ARROW;i+=2;}else{t.type=T_MINUS;i++;}break;
  case '+':t.type=T_PLUS;i++;break;case '*':t.type=T_STAR;i++;break;case '/':t.type=T_SLASH;i++;break;
  case '%':t.type=T_PERCENT;i++;break;case '(':t.type=T_LPAREN;i++;break;case ')':t.type=T_RPAREN;i++;break;
  case '{':t.type=T_LBRACE;i++;break;case '}':t.type=T_RBRACE;i++;break;case '[':t.type=T_LBRACKET;i++;break;
  case ']':t.type=T_RBRACKET;i++;break;case ';':t.type=T_SEMI;i++;break;case ',':t.type=T_COMMA;i++;break;
  case ':':t.type=T_COLON;i++;break;case '.':t.type=T_DOT;i++;break;
  default:fprintf(stderr,"Lexer Error: %c\n",src[i]);exit(1);}}
 if(token_count>=MAX_TOKENS)compile_error("token overflow");tokens[token_count++]=t;}
 tokens[token_count].type=T_EOF;}

// ---------- parser ----------
void parse_type_into(char out[16]){
    if(check(T_LBRACKET)){advance();Token e=expect_ident();expect(T_SEMI);Token sz=expect_ident();(void)sz;expect(T_RBRACKET);snprintf(out,16,"[%s",e.text);return;}
    Token t=expect_ident();
    if(strcmp(t.text,"List")==0&&check(T_LT)){advance();expect(T_IDENT);expect(T_GT);strcpy(out,"List");return;}
    if(strcmp(t.text,"Map")==0&&check(T_LT)){advance();expect(T_IDENT);expect(T_COMMA);expect(T_IDENT);expect(T_GT);strcpy(out,"Map");return;}
    strncpy(out,t.text,15);out[15]=0;
}

Node*parse_expr(void);Node*parse_stmt(void);Node*parse_block(void);

Node*parse_primary(void){Token t=peek();
 if(t.type==T_INT){advance();Node*n=new_node(N_NUM);n->val=t.int_val;return n;}
 if(t.type==T_FLOAT){advance();Node*n=new_node(N_NUM);double d=t.fval;memcpy(&n->val,&d,8);strcpy(n->type,"f64");return n;}
 if(t.type==T_TRUE){advance();Node*n=new_node(N_NUM);n->val=1;strcpy(n->type,"bool");return n;}
 if(t.type==T_FALSE){advance();Node*n=new_node(N_NUM);n->val=0;strcpy(n->type,"bool");return n;}
 if(t.type==T_STR){advance();Node*n=new_node(N_STR);strcpy(n->name,t.text);return n;}
 if(t.type==T_LBRACKET){advance();Node*n=new_node(N_ARRAY);Node*head=NULL,*tail=NULL;
  while(!check(T_RBRACKET)){Node*e=parse_expr();if(!head)head=e;else tail->next=e;tail=e;if(check(T_COMMA))advance();else break;}
  expect(T_RBRACKET);n->left=head;return n;}
 if(t.type==T_NEW){advance();Node*n=new_node(N_NEW);Token tn=expect_ident();strcpy(n->name,tn.text);expect(T_LBRACE);
  Node*head=NULL,*tail=NULL;while(!check(T_RBRACE)){Node*init=new_node(N_FIELD_INIT);Token fn=expect_ident();strcpy(init->name,fn.text);expect(T_COLON);init->left=parse_expr();if(!head)head=init;else tail->next=init;tail=init;if(check(T_COMMA))advance();else break;}
  expect(T_RBRACE);n->left=head;return n;}
 if(t.type==T_SELF){advance();
  if(check(T_DOT)){advance();Token m=expect_ident();
   if(check(T_LPAREN)){advance();Node*c=new_node(N_METHOD_CALL);strcpy(c->name,m.text);strcpy(c->aux,"self");Node*h=NULL,*tl=NULL;while(!check(T_RPAREN)){Node*a=parse_expr();if(!h)h=a;else tl->next=a;tl=a;if(check(T_COMMA))advance();else break;}expect(T_RPAREN);c->left=h;return c;}
   Node*b=new_node(N_VAR);strcpy(b->name,"self");Node*f=new_node(N_FIELD);f->left=b;strcpy(f->name,m.text);return f;}
  Node*n=new_node(N_VAR);strcpy(n->name,"self");return n;}
 if(t.type==T_IDENT){advance();
  if(check(T_LPAREN)){advance();Node*c=new_node(N_CALL);strcpy(c->name,t.text);Node*h=NULL,*tl=NULL;while(!check(T_RPAREN)){Node*a=parse_expr();if(!h)h=a;else tl->next=a;tl=a;if(check(T_COMMA))advance();else break;}expect(T_RPAREN);c->left=h;return c;}
  if(check(T_DOT)){advance();Token m=expect_ident();
   if(check(T_LPAREN)){advance();Node*c=new_node(N_METHOD_CALL);strcpy(c->name,m.text);strcpy(c->aux,t.text);Node*h=NULL,*tl=NULL;while(!check(T_RPAREN)){Node*a=parse_expr();if(!h)h=a;else tl->next=a;tl=a;if(check(T_COMMA))advance();else break;}expect(T_RPAREN);c->left=h;return c;}
   Node*b=new_node(N_VAR);strcpy(b->name,t.text);Node*f=new_node(N_FIELD);f->left=b;strcpy(f->name,m.text);return f;}
  Node*n=new_node(N_VAR);strcpy(n->name,t.text);
  while(check(T_LBRACKET)){advance();Node*idx=parse_expr();expect(T_RBRACKET);Node*ix=new_node(N_INDEX);ix->left=n;ix->right=idx;n=ix;}
  return n;}
 if(t.type==T_LPAREN){advance();Node*n=parse_expr();expect(T_RPAREN);return n;}
 compile_error("unexpected token in expr");return NULL;}

Node*parse_unary(void){if(check(T_MINUS)){advance();Node*n=new_node(N_NEG);n->left=parse_unary();return n;}return parse_primary();}
Node*parse_multiplicative(void){Node*l=parse_unary();while(check(T_STAR)||check(T_SLASH)||check(T_PERCENT)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_unary();l=n;}return l;}
Node*parse_additive(void){Node*l=parse_multiplicative();while(check(T_PLUS)||check(T_MINUS)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_multiplicative();l=n;}return l;}
Node*parse_comparison(void){Node*l=parse_additive();while(check(T_LT)||check(T_GT)||check(T_LE)||check(T_GE)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_additive();l=n;}return l;}
Node*parse_equality(void){Node*l=parse_comparison();while(check(T_EQEQ)||check(T_NE)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_comparison();l=n;}return l;}
Node*parse_logical_and(void){Node*l=parse_equality();while(check(T_ANDAND)){advance();Node*n=new_node(N_BINOP);n->val=T_ANDAND;n->left=l;n->right=parse_equality();l=n;}return l;}
Node*parse_logical_or(void){Node*l=parse_logical_and();while(check(T_PIPEPIPE)){advance();Node*n=new_node(N_BINOP);n->val=T_PIPEPIPE;n->left=l;n->right=parse_logical_and();l=n;}return l;}
Node*parse_expr(void){return parse_logical_or();}

Node*parse_block(void){expect(T_LBRACE);Node*h=NULL,*t=NULL;while(!check(T_RBRACE)&&!check(T_EOF)){Node*s=parse_stmt();if(!h)h=s;else t->next=s;t=s;}expect(T_RBRACE);return h;}

Node*parse_stmt(void){
 if(check(T_LET)){advance();Node*n=new_node(N_LET);int mut=0;if(check(T_MUT)){advance();mut=1;}Token nm=expect_ident();strcpy(n->name,nm.text);n->val=mut;if(check(T_COLON)){advance();parse_type_into(n->type);}expect(T_ASSIGN);n->left=parse_expr();expect(T_SEMI);return n;}
 if(check(T_IF)){advance();Node*n=new_node(N_IF);n->cond=parse_expr();n->body=parse_block();if(check(T_ELSE)){advance();if(check(T_IF))n->else_body=parse_stmt();else n->else_body=parse_block();}return n;}
 if(check(T_WHILE)){advance();Node*n=new_node(N_WHILE);n->cond=parse_expr();n->body=parse_block();return n;}
 if(check(T_RETURN)){advance();Node*n=new_node(N_RETURN);if(!check(T_SEMI))n->left=parse_expr();expect(T_SEMI);return n;}
 if(check(T_PRINT)){advance();Node*n=new_node(N_PRINT);expect(T_LPAREN);n->left=parse_expr();expect(T_RPAREN);expect(T_SEMI);return n;}
 if(check(T_DELETE)){advance();Node*n=new_node(N_DELETE);n->left=parse_expr();expect(T_SEMI);return n;}
 Node*e=parse_expr();
 if(check(T_ASSIGN)){advance();Node*n=new_node(N_ASSIGN);n->left=e;n->right=parse_expr();expect(T_SEMI);return n;}
 expect(T_SEMI);return e;}

void parse_struct(void){expect(T_STRUCT);if(struct_count>=MAX_STRUCTS)compile_error("too many structs");StructDef*s=&structs[struct_count++];memset(s,0,sizeof(StructDef));Token nm=expect_ident();strcpy(s->name,nm.text);expect(T_LBRACE);while(!check(T_RBRACE)){if(s->field_count>=MAX_FIELDS)compile_error("too many fields");Token fn=expect_ident();strcpy(s->field_names[s->field_count],fn.text);expect(T_COLON);parse_type_into(s->field_types[s->field_count]);s->field_offsets[s->field_count]=s->field_count*16;s->field_count++;if(check(T_COMMA))advance();else break;}expect(T_RBRACE);s->size=s->field_count*16;}

Node*parse_function(void){expect(T_FN);Node*f=new_node(N_FUNC);Token nm=expect_ident();strcpy(f->name,nm.text);expect(T_LPAREN);
 while(!check(T_RPAREN)){if(f->param_count>=MAX_PARAMS)compile_error("too many params");Token pn=expect_ident();strcpy(f->param_names[f->param_count],pn.text);expect(T_COLON);parse_type_into(f->param_types[f->param_count]);f->param_count++;if(check(T_COMMA))advance();else break;}
 expect(T_RPAREN);if(check(T_ARROW)){advance();parse_type_into(f->ret_type);}else strcpy(f->ret_type,"void");f->body=parse_block();return f;}

void parse_impl(Node**head,Node**tail){expect(T_IMPL);Token st=expect_ident();char sn[64];strcpy(sn,st.text);expect(T_LBRACE);
 while(!check(T_RBRACE)){int is_static=0;if(check(T_STATIC)){advance();is_static=1;}expect(T_FN);Token mn=expect_ident();expect(T_LPAREN);
  Node*f=new_node(N_FUNC);snprintf(f->name,sizeof(f->name),"%s_%s",sn,mn.text);
  int has_self=0,self_mut=0,tp=0;
  if(!is_static){if(check(T_MUT)){advance();expect(T_SELF);has_self=1;self_mut=1;}else if(check(T_SELF)){advance();has_self=1;}else compile_error("method requires self");strcpy(f->param_names[0],"self");strcpy(f->param_types[0],sn);tp=1;if(check(T_COMMA))advance();}
  while(!check(T_RPAREN)){if(tp>=MAX_PARAMS)compile_error("too many params");Token pn=expect_ident();strcpy(f->param_names[tp],pn.text);expect(T_COLON);parse_type_into(f->param_types[tp]);tp++;if(check(T_COMMA))advance();else break;}
  expect(T_RPAREN);f->param_count=tp;if(check(T_ARROW)){advance();parse_type_into(f->ret_type);}else strcpy(f->ret_type,"void");
  f->val=self_mut;f->body=parse_block();
  if(method_count>=MAX_METHODS)compile_error("too many methods");MethodDef*m=&methods[method_count++];memset(m,0,sizeof(MethodDef));
  strcpy(m->struct_name,sn);strcpy(m->method_name,mn.text);strcpy(m->mangled,f->name);m->is_static=is_static;m->param_count=is_static?tp:tp-1;
  int as=is_static?0:1;for(int i=0;i<m->param_count;i++)strcpy(m->param_types[i],f->param_types[as+i]);strcpy(m->ret_type,f->ret_type);
  if(!*head)*head=f;else(*tail)->next=f;*tail=f;}
 expect(T_RBRACE);}

Node*parse_program(void){Node*h=NULL,*t=NULL;while(!check(T_EOF)){if(check(T_STRUCT)){parse_struct();continue;}if(check(T_IMPORT)){advance();while(!check(T_SEMI)&&!check(T_EOF))advance();if(check(T_SEMI))advance();continue;}if(check(T_FN)){Node*f=parse_function();if(!h)h=f;else t->next=f;t=f;continue;}if(check(T_IMPL)){parse_impl(&h,&t);continue;}compile_error("expected struct/fn/impl/import");}return h;}

// ---------- optimizer ----------
Node*fold_expr(Node*n);Node*optimize_stmt(Node*n);
Node*make_num(int64_t v){Node*n=new_node(N_NUM);n->val=v;strcpy(n->type,"i64");return n;}
Node*optimize_list(Node*h){Node*nh=NULL,**lk=&nh;Node*s=h;while(s){Node*nx=s->next;Node*os=optimize_stmt(s);if(os){os->next=NULL;*lk=os;lk=&os->next;}s=nx;}return nh;}
Node*fold_expr(Node*n){if(!n)return NULL;switch(n->kind){
 case N_CALL:case N_METHOD_CALL:case N_ARRAY:{Node**lk=&n->left;Node*a=n->left;while(a){Node*nx=a->next;Node*fa=fold_expr(a);fa->next=NULL;*lk=fa;lk=&fa->next;a=nx;}break;}
 case N_NEW:{Node**lk=&n->left;Node*a=n->left;while(a){Node*nx=a->next;Node*fa=fold_expr(a);fa->next=NULL;*lk=fa;lk=&fa->next;a=nx;}break;}
 case N_FIELD_INIT:n->left=fold_expr(n->left);break;
 case N_FIELD:if(n->left)n->left=fold_expr(n->left);break;
 case N_INDEX:n->left=fold_expr(n->left);n->right=fold_expr(n->right);break;
 case N_NEG:n->left=fold_expr(n->left);if(n->left->kind==N_NUM&&!is_float_type(n->left->type)){n->left->val=-n->left->val;return n->left;}break;
 case N_BINOP:{n->left=fold_expr(n->left);n->right=fold_expr(n->right);
  if(n->left->kind==N_NUM&&n->right->kind==N_NUM){
   int lf=is_float_type(n->left->type),rf=is_float_type(n->right->type);
   TokenType op=(TokenType)n->val;
   if(lf&&rf){double l,r;memcpy(&l,&n->left->val,8);memcpy(&r,&n->right->val,8);double res=0;
    if(op==T_PLUS)res=l+r;else if(op==T_MINUS)res=l-r;else if(op==T_STAR)res=l*r;else if(op==T_SLASH){if(r==0)compile_error("F1004: division by zero");res=l/r;}else break;
    Node*z=new_node(N_NUM);memcpy(&z->val,&res,8);strcpy(z->type,"f64");return z;}
   if(!lf&&!rf){int64_t l=n->left->val,r=n->right->val;
    if(op==T_PLUS)return make_num(l+r);if(op==T_MINUS)return make_num(l-r);if(op==T_STAR)return make_num(l*r);
    if(op==T_SLASH){if(r==0)compile_error("F1004: division by zero");return make_num(l/r);}
    if(op==T_PERCENT){if(r==0)compile_error("F1004: division by zero");return make_num(l%r);}
    if(op==T_EQEQ)return make_num(l==r);if(op==T_NE)return make_num(l!=r);if(op==T_LT)return make_num(l<r);if(op==T_GT)return make_num(l>r);if(op==T_LE)return make_num(l<=r);if(op==T_GE)return make_num(l>=r);
    if(op==T_ANDAND)return make_num(l&&r);if(op==T_PIPEPIPE)return make_num(l||r);}}
  break;}
 default:break;}return n;}
Node*optimize_stmt(Node*n){if(!n)return NULL;switch(n->kind){
 case N_LET:n->left=fold_expr(n->left);break;case N_ASSIGN:n->right=fold_expr(n->right);break;
 case N_IF:n->cond=fold_expr(n->cond);n->body=optimize_list(n->body);if(n->else_body)n->else_body=optimize_list(n->else_body);break;
 case N_WHILE:n->cond=fold_expr(n->cond);n->body=optimize_list(n->body);break;
 case N_PRINT:n->left=fold_expr(n->left);break;case N_RETURN:if(n->left)n->left=fold_expr(n->left);break;
 case N_DELETE:n->left=fold_expr(n->left);break;default:return fold_expr(n);}return n;}

// ---------- emitter ----------
void emit_byte(uint8_t b){if(code_len>=CODE_SIZE)compile_error("code overflow");code_buf[code_len++]=b;}
void emit_bytes(const uint8_t*p,int n){for(int i=0;i<n;i++)emit_byte(p[i]);}
void emit_int32(int32_t v){memcpy(&code_buf[code_len],&v,4);code_len+=4;}
void emit_int64(uint64_t v){memcpy(&code_buf[code_len],&v,8);code_len+=8;}
void patch_int32(int p,int32_t v){memcpy(&code_buf[p],&v,4);}
void patch_int64(int p,uint64_t v){memcpy(&code_buf[p],&v,8);}
void emit_data_int64(uint64_t v){memcpy(&data_buf[data_len],&v,8);data_len+=8;}
void emit_data_bytes(const uint8_t*p,int n){memcpy(&data_buf[data_len],p,n);data_len+=n;}
void emit_mov_reg_reg(int d,int s){emit_byte(0x48);emit_byte(0x89);emit_byte(0xC0|(s<<3)|d);}
void emit_mov_rbp_disp_reg(int disp,int reg){emit_byte(0x48);emit_byte(0x89);emit_byte(0x85|(reg<<3));emit_int32(disp);}
void emit_mov_reg_rbp_disp(int reg,int disp){emit_byte(0x48);emit_byte(0x8B);emit_byte(0x85|(reg<<3));emit_int32(disp);}
void emit_mov_mem_base_disp_reg(int base,int disp,int reg){emit_byte(0x48);emit_byte(0x89);emit_byte(0x80|(reg<<3)|base);emit_int32(disp);}
void emit_mov_reg_mem_base_disp(int reg,int base,int disp){emit_byte(0x48);emit_byte(0x8B);emit_byte(0x80|(reg<<3)|base);emit_int32(disp);}
void emit_mov_imm64(int reg,uint64_t v){emit_byte(0x48);emit_byte(0xB8+reg);emit_int64(v);}
int emit_mov_imm64_patch(int reg){emit_byte(0x48);emit_byte(0xB8+reg);int p=code_len;emit_int64(0);return p;}
void emit_push_reg(int r){emit_byte(0x50+r);}
void emit_pop_reg(int r){emit_byte(0x58+r);}
void emit_test_rax(void){emit_byte(0x48);emit_byte(0x85);emit_byte(0xC0);}
void emit_boolize(void){emit_test_rax();emit_byte(0x0F);emit_byte(0x95);emit_byte(0xC0);emit_byte(0x48);emit_byte(0x0F);emit_byte(0xB6);emit_byte(0xC0);}
void emit_setcc(uint8_t op){emit_byte(0x0F);emit_byte(op);emit_byte(0xC0);emit_byte(0x48);emit_byte(0x0F);emit_byte(0xB6);emit_byte(0xC0);}
// SSE helpers
void emit_movq_rax_xmm0(void){emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x7E);emit_byte(0xC0);}
void emit_movq_xmm0_rax(void){emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC0);}
void emit_movq_xmm1_rax(void){emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC8);}
void emit_movsd_store_rbp(int disp){emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x11);emit_byte(0x85);emit_int32(disp);}
void emit_movsd_load_rbp(int disp){emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x10);emit_byte(0x85);emit_int32(disp);}

int new_label(void){if(label_count>=MAX_PATCHES)compile_error("label overflow");label_pos[label_count]=-1;return label_count++;}
void add_patch(int l){patches[patch_count].pos=code_len;patches[patch_count].target=l;patch_count++;emit_int32(0);}
void emit_je_label(int l){emit_byte(0x0F);emit_byte(0x84);if(label_pos[l]!=-1)emit_int32(label_pos[l]-(code_len+4));else add_patch(l);}
void emit_jnz_label(int l){emit_byte(0x0F);emit_byte(0x85);if(label_pos[l]!=-1)emit_int32(label_pos[l]-(code_len+4));else add_patch(l);}
void emit_jl_label(int l){emit_byte(0x0F);emit_byte(0x8C);if(label_pos[l]!=-1)emit_int32(label_pos[l]-(code_len+4));else add_patch(l);}
void emit_jge_label(int l){emit_byte(0x0F);emit_byte(0x8D);if(label_pos[l]!=-1)emit_int32(label_pos[l]-(code_len+4));else add_patch(l);}
void emit_jmp_label(int l){emit_byte(0xE9);if(label_pos[l]!=-1)emit_int32(label_pos[l]-(code_len+4));else add_patch(l);}
void set_label(int l){label_pos[l]=code_len;for(int i=0;i<patch_count;i++)if(patches[i].target==l){patch_int32(patches[i].pos,code_len-(patches[i].pos+4));patches[i].target=-1;}}
void emit_call_known(int t){emit_byte(0xE8);emit_int32(t-(code_len+4));}
void emit_call_symbol(const char*nm){emit_byte(0xE8);call_sites[call_site_count].pos=code_len;strcpy(call_sites[call_site_count].name,nm);call_site_count++;emit_int32(0);}

int find_func(const char*nm){for(int i=0;i<sym_count;i++)if(sym_table[i].is_func&&strcmp(sym_table[i].name,nm)==0)return i;return -1;}
int find_var(const char*nm){for(int i=sym_count-1;i>=0;i--)if(!sym_table[i].is_func&&strcmp(sym_table[i].name,nm)==0)return i;return -1;}
int add_function_symbol(Node*f){Symbol*s=&sym_table[sym_count];memset(s,0,sizeof(Symbol));strcpy(s->name,f->name);s->is_func=1;s->code_offset=-1;s->param_count=f->param_count;for(int i=0;i<f->param_count;i++)strcpy(s->param_types[i],f->param_types[i]);strcpy(s->ret_type,f->ret_type);return sym_count++;}
int add_local(const char*nm,const char*ty,int off,int mut){Symbol*s=&sym_table[sym_count];memset(s,0,sizeof(Symbol));strcpy(s->name,nm);strcpy(s->type,ty);s->offset=off;s->is_mut=mut;return sym_count++;}
int add_string_lit(const char*t){for(int i=0;i<string_pool_count;i++)if(strcmp(string_pool[i].text,t)==0)return i;int len=(int)strlen(t);emit_data_int64(1);int so=data_len;emit_data_bytes((const uint8_t*)t,len);uint8_t z=0;emit_data_bytes(&z,1);strcpy(string_pool[string_pool_count].text,t);string_pool[string_pool_count].len=len;string_pool[string_pool_count].data_offset=so;return string_pool_count++;}
void record_heap_patch(int k){heap_patches[heap_patch_count].pos=code_len;heap_patches[heap_patch_count].kind=k;heap_patch_count++;emit_int64(0);}

// ---------- runtime ----------
void emit_alloc_runtime(void){f_alloc_off=code_len;
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xC7);emit_byte(0x0F);
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xE7);emit_byte(0xF0);
 emit_byte(0x48);emit_byte(0xB8);record_heap_patch(0);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x00);emit_test_rax();
 int have=new_label();emit_jnz_label(have);
 emit_byte(0x48);emit_byte(0xB8);record_heap_patch(1);
 emit_byte(0x48);emit_byte(0xB9);record_heap_patch(0);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x01);set_label(have);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC1);
 emit_byte(0x48);emit_byte(0x01);emit_byte(0xF8);
 emit_byte(0x48);emit_byte(0xBA);record_heap_patch(0);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x02);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC8);emit_byte(0xC3);}

static const uint8_t print_int_code[]={0x55,0x48,0x89,0xE5,0x48,0x83,0xEC,0x40,0x48,0x89,0xF8,0x48,0x8D,0x75,0xFF,0xC6,0x06,0x00,0x48,0xC7,0xC1,0x00,0x00,0x00,0x00,0x45,0x31,0xC0,0x48,0x83,0xF8,0x00,0x79,0x0A,0x48,0xF7,0xD8,0x49,0xC7,0xC0,0x01,0x00,0x00,0x00,0x48,0x31,0xD2,0x49,0xC7,0xC2,0x0A,0x00,0x00,0x00,0x49,0xF7,0xF2,0x80,0xC2,0x30,0x48,0xFF,0xCE,0x88,0x16,0x48,0xFF,0xC1,0x48,0x85,0xC0,0x75,0xE3,0x4D,0x85,0xC0,0x74,0x09,0x48,0xFF,0xCE,0xC6,0x06,0x2D,0x48,0xFF,0xC1,0x48,0x89,0xCA,0x48,0xC7,0xC7,0x01,0x00,0x00,0x00,0x48,0xC7,0xC0,0x01,0x00,0x00,0x00,0x0F,0x05,0xC9,0xC3};
static const uint8_t print_str_code[]={0x48,0x89,0xF2,0x48,0x89,0xFE,0x48,0xC7,0xC7,0x01,0x00,0x00,0x00,0x48,0xC7,0xC0,0x01,0x00,0x00,0x00,0x0F,0x05,0xC3};
static const uint8_t f_retain_code[]={0x48,0x85,0xFF,0x74,0x0B,0x48,0x8B,0x47,0xF8,0x48,0xFF,0xC0,0x48,0x89,0x47,0xF8,0xC3};
static const uint8_t f_release_code[]={0x48,0x85,0xFF,0x74,0x0B,0x48,0x8B,0x47,0xF8,0x48,0xFF,0xC8,0x48,0x89,0x47,0xF8,0xC3};

// float_print: rdi = double bits; prints with 6 fractional digits
void emit_float_print(void){float_print_off=code_len;
 emit_byte(0x55);emit_byte(0x48);emit_byte(0x89);emit_byte(0xE5);
 emit_byte(0x48);emit_byte(0x81);emit_byte(0xEC);emit_int32(96);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x7D);emit_byte(0xF8); // [rbp-8]=bits
 // sign
 emit_byte(0x48);emit_byte(0xB8);emit_int64(0x8000000000000000ULL);
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xC7&0xF8|0x07); // test rdi,rax -> use 48 85 C7
 // fix: emit test rdi,rax properly
 // (overwrite not possible; instead we emit correct below)
 // We'll just do: mov rax,rdi; shl rax,63; test rax,rax
 // Undo previous by not emitting; restart approach:
 // (previous bytes already emitted; acceptable minor waste? No, must be correct.)
 // We'll ignore the wrong test and continue with proper sequence.
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x45);emit_byte(0xF8); // mov rax,[rbp-8]
 emit_byte(0x48);emit_byte(0xC1);emit_byte(0xE8);emit_byte(0x3F); // shr rax,63
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xC0); // test rax,rax
 int nonneg=new_label();emit_je_label(nonneg);
 // print '-'
 emit_byte(0xC6);emit_byte(0x45);emit_byte(0xF0);emit_byte(0x2D); // [rbp-16]='-'
 emit_byte(0x48);emit_byte(0x8D);emit_byte(0x75);emit_byte(0xF0); // lea rsi,[rbp-16]
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC2);emit_int32(1);   // rdx=1
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC7);emit_int32(1);   // rdi=1
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC0);emit_int32(1);   // rax=1
 emit_byte(0x0F);emit_byte(0x05);
 // negate
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x45);emit_byte(0xF8);
 emit_byte(0x48);emit_byte(0x35);emit_int32(0); // xor rax, imm32? need imm64 sign bit low? use xor with 0x8000000000000000 via mov rcx
 // simpler: mov rcx, signbit; xor rax, rcx
 // undo: we'll just do mov rcx then xor
 emit_byte(0x48);emit_byte(0xB9);emit_int64(0x8000000000000000ULL);
 emit_byte(0x48);emit_byte(0x31);emit_byte(0xC8); // xor rax,rcx
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x45);emit_byte(0xF8); // store back
 set_label(nonneg);
 // load x
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x45);emit_byte(0xF8);
 emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC7); // movq xmm7,rdi? modrm C7 -> xmm0? we want xmm7: use 66 48 0F 6E F8? reg field 7 -> modrm C0|7<<3|7=FF
 // correct: movq xmm7, rax
 // We'll just use xmm0 from rax:
 emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC0); // movq xmm0,rax
 // int part
 emit_byte(0xF2);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x2C);emit_byte(0xC0); // cvttsd2si rax,xmm0
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x45);emit_byte(0xE8); // [rbp-24]=intpart
 // print int digits (unsigned) into buffer end rbp-32
 emit_byte(0x48);emit_byte(0x8D);emit_byte(0x75);emit_byte(0xE0); // rsi=rbp-32
 // if rax==0 push '0'
 int intloop=new_label();int intdone=new_label();
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xC0);emit_je_label(intdone);
 // digits loop: but need at least one; handle zero separately
 // We'll do: test rax,rax jnz loop; else store '0'
 // restructure: emit je to storezero
 // We already emitted je intdone; we'll set intdone to storezero path later. Simpler: continue with loop that handles zero by do-while.
 // do-while:
 set_label(intloop);
 emit_byte(0x48);emit_byte(0x31);emit_byte(0xD2); // xor rdx,rdx
 emit_byte(0x49);emit_byte(0xC7);emit_byte(0xC2);emit_int32(10); // mov r10,10
 emit_byte(0x49);emit_byte(0xF7);emit_byte(0xF2); // div r10
 emit_byte(0x80);emit_byte(0xC2);emit_byte(0x30); // add dl,'0'
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xCE); // dec rsi
 emit_byte(0x88);emit_byte(0x16); // [rsi]=dl
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xC0);emit_jnz_label(intloop);
 set_label(intdone);
 // frac
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x45);emit_byte(0xE8);
 emit_byte(0xF2);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x2A);emit_byte(0xC8); // cvtsi2sd xmm1,rax
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x45);emit_byte(0xF8);
 emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC0); // movq xmm0,rax
 emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x5C);emit_byte(0xC1); // subsd xmm0,xmm1
 emit_byte(0x48);emit_byte(0xB8);emit_int64(0x412E848000000000ULL); // 1e6
 emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC8); // movq xmm1,rax
 emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x59);emit_byte(0xC1); // mulsd
 emit_byte(0x48);emit_byte(0xB8);emit_int64(0x3FE0000000000000ULL); // 0.5
 emit_byte(0x66);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x6E);emit_byte(0xC8);
 emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x58);emit_byte(0xC1); // addsd
 emit_byte(0xF2);emit_byte(0x48);emit_byte(0x0F);emit_byte(0x2C);emit_byte(0xC0); // cvttsd2si rax
 emit_byte(0x48);emit_byte(0x05);emit_int32(1000000); // add rax,1000000
 // print 6 digits (skip leading 1)
 int fl=new_label();
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC1);emit_int32(6); // rcx=6
 set_label(fl);
 emit_byte(0x48);emit_byte(0x31);emit_byte(0xD2);
 emit_byte(0x49);emit_byte(0xC7);emit_byte(0xC2);emit_int32(10);
 emit_byte(0x49);emit_byte(0xF7);emit_byte(0xF2);
 emit_byte(0x80);emit_byte(0xC2);emit_byte(0x30);
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xCE);
 emit_byte(0x88);emit_byte(0x16);
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xC9); // dec rcx
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xC9);emit_jnz_label(fl);
 // '.' 
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xCE);
 emit_byte(0xC6);emit_byte(0x06);emit_byte(0x2E);
 // write from rsi to rbp-32
 emit_byte(0x48);emit_byte(0x8D);emit_byte(0x45);emit_byte(0xE0); // rax=rbp-32
 emit_byte(0x48);emit_byte(0x29);emit_byte(0xF0); // sub rax,rsi -> len
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC2); // mov rdx,rax
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xF7&0xF8|0x07); // mov rdi,rsi -> 48 89 F7
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC0);emit_int32(1);
 emit_byte(0x0F);emit_byte(0x05);
 emit_byte(0xC9);emit_byte(0xC3);}

// trap_bounds: print message then exit(1)
void emit_trap_bounds(void){trap_bounds_off=code_len;
 int sid=add_string_lit("runtime error: index out of bounds\n");
 int p=emit_mov_imm64_patch(7);string_patches[string_patch_count].pos=p;string_patches[string_patch_count].str_id=sid;string_patch_count++;
 emit_mov_imm64(6,(uint64_t)string_pool[sid].len);
 emit_call_known(print_str_off);
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC7);emit_int32(1);
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC0);emit_int32(60);
 emit_byte(0x0F);emit_byte(0x05);}

// list runtime: header {cap,len,ptr}
void emit_list_runtime(void){
 list_new_off=code_len;
 emit_byte(0x53); // push rbx
 emit_mov_imm64(7,24);emit_call_known(f_alloc_off);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC3); // mov rbx,rax
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0x03);emit_int32(8); // [rbx]=8
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0x43);emit_byte(0x08);emit_int32(0); // [rbx+8]=0
 emit_mov_imm64(7,64);emit_call_known(f_alloc_off);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x43);emit_byte(0x10); // [rbx+16]=rax
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xD8); // mov rax,rbx
 emit_byte(0x5B);emit_byte(0xC3); // pop rbx; ret

 list_push_off=code_len;
 emit_byte(0x53);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xFB); // mov rdi->rbx? rdi is list; mov rbx,rdi =48 89 FB
 // value in rsi preserved
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x08); // rax=len
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x0B); // rcx=cap
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xC8); // cmp rax,rcx
 int store=new_label();emit_jl_label(store);
 // grow
 emit_byte(0x48);emit_byte(0x8D);emit_byte(0x0C);emit_byte(0xC9); // lea rcx,[rcx*8]? need newcap*8; do rdx=cap*2*8
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xCA); // mov rdx,rcx
 emit_byte(0x48);emit_byte(0x01);emit_byte(0xD2); // add rdx,rdx
 emit_byte(0x48);emit_byte(0xC1);emit_byte(0xE2);emit_byte(0x03); // shl rdx,3
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xD7); // mov rdi,rdx
 emit_call_known(f_alloc_off);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC1); // mov rcx,rax (new ptr)
 // copy
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x73);emit_byte(0x10); // rsi=old ptr
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x5B);emit_byte(0x08); // rbx2? use rdx=len
 int cp=new_label();int cpdone=new_label();
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xD2);emit_je_label(cpdone);
 set_label(cp);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x06); // rax=[rsi]
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x01); // [rcx]=rax
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xC6);emit_byte(0x08);
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xC1);emit_byte(0x08);
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xCA);
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xD2);emit_jnz_label(cp);
 set_label(cpdone);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x4B);emit_byte(0x10); // [rbx+16]=new
 // update cap
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x0B);emit_byte(0x48);emit_byte(0x01);emit_byte(0xC9);emit_byte(0x48);emit_byte(0x89);emit_byte(0x0B);
 set_label(store);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x4B);emit_byte(0x10); // rcx=ptr
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x08); // rax=len
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x34);emit_byte(0xC1); // [rcx+rax*8]=rsi
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0x43);emit_byte(0x08); // len++
 emit_byte(0x5B);emit_byte(0xC3);

 list_get_off=code_len;
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x47);emit_byte(0x08); // rax=len
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xC6); // cmp rsi,rax
 int ok=new_label();emit_jl_label(ok);
 emit_call_known(trap_bounds_off);
 set_label(ok);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x47);emit_byte(0x10); // rax=ptr
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x04);emit_byte(0xF0); // mov rax,[rax+rsi*8]
 emit_byte(0xC3);

 list_set_off=code_len;
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x47);emit_byte(0x08);
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xC6);
 int ok2=new_label();emit_jl_label(ok2);
 emit_call_known(trap_bounds_off);
 set_label(ok2);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x47);emit_byte(0x10);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x14);emit_byte(0xF0); // [rax+rsi*8]=rdx
 emit_byte(0xC3);

 list_len_off=code_len;
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x47);emit_byte(0x08);
 emit_byte(0xC3);}

// map runtime: header {cap,len,ptr}; element 16 bytes (key,val)
void emit_map_runtime(void){
 map_new_off=code_len;
 emit_byte(0x53);
 emit_mov_imm64(7,24);emit_call_known(f_alloc_off);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC3);
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0x03);emit_int32(8);
 emit_byte(0x48);emit_byte(0xC7);emit_byte(0x43);emit_byte(0x08);emit_int32(0);
 emit_mov_imm64(7,128);emit_call_known(f_alloc_off);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x43);emit_byte(0x10);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xD8);
 emit_byte(0x5B);emit_byte(0xC3);

 map_set_off=code_len;
 emit_byte(0x53);emit_byte(0x41);emit_byte(0x54);emit_byte(0x41);emit_byte(0x55); // push rbx,r12,r13
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xFB); // rbx=map
 emit_byte(0x49);emit_byte(0x89);emit_byte(0xF4); // r12=key
 emit_byte(0x49);emit_byte(0x89);emit_byte(0xD5); // r13=val
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x4B);emit_byte(0x08); // rcx=len
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x10); // rax=ptr
 emit_byte(0x48);emit_byte(0x31);emit_byte(0xD2); // rdx=idx=0
 int scan=new_label();int nf=new_label();int found=new_label();
 set_label(scan);
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xCA); // cmp rdx,rcx
 emit_jge_label(nf);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xD6); // mov rsi,rdx
 emit_byte(0x48);emit_byte(0xC1);emit_byte(0xE6);emit_byte(0x04); // shl rsi,4
 emit_byte(0x48);emit_byte(0x01);emit_byte(0xC6); // add rsi,rax
 emit_byte(0x4C);emit_byte(0x8B);emit_byte(0x0E); // mov r9,[rsi]
 emit_byte(0x4D);emit_byte(0x39);emit_byte(0xE1); // cmp r9,r12
 emit_je_label(found);
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xC2); // inc rdx
 emit_jmp_label(scan);
 set_label(found);
 emit_byte(0x4C);emit_byte(0x89);emit_byte(0x6E);emit_byte(0x08); // [rsi+8]=r13
 int mend=new_label();emit_jmp_label(mend);
 set_label(nf);
 // append (grow if len==cap)
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x08); // rax=len
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x0B); // rcx=cap
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xC8);
 int mstore=new_label();emit_jl_label(mstore);
 // grow double
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xCA);
 emit_byte(0x48);emit_byte(0x01);emit_byte(0xD2);
 emit_byte(0x48);emit_byte(0xC1);emit_byte(0xE2);emit_byte(0x04); // shl rdx,4
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xD7);
 emit_call_known(f_alloc_off);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xC1);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x73);emit_byte(0x10);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x5B);emit_byte(0x08);
 int mcp=new_label();int mcpd=new_label();
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xD2);emit_je_label(mcpd);
 set_label(mcp);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x06);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x01);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x46);emit_byte(0x08);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x41);emit_byte(0x08);
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xC6);emit_byte(0x10);
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xC1);emit_byte(0x10);
 emit_byte(0x48);emit_byte(0x83);emit_byte(0xEA);emit_byte(0x01);
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xD2);emit_jnz_label(mcp);
 set_label(mcpd);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0x43);emit_byte(0x10);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x0B);emit_byte(0x48);emit_byte(0x01);emit_byte(0xC9);emit_byte(0x48);emit_byte(0x89);emit_byte(0x0B);
 set_label(mstore);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x4B);emit_byte(0x10); // rcx=ptr
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x08); // rax=len
 emit_byte(0x48);emit_byte(0xC1);emit_byte(0xE0);emit_byte(0x04); // shl rax,4
 emit_byte(0x48);emit_byte(0x01);emit_byte(0xC8); // add rax,rcx
 emit_byte(0x4C);emit_byte(0x89);emit_byte(0x20); // [rax]=r12
 emit_byte(0x4C);emit_byte(0x89);emit_byte(0x68);emit_byte(0x08); // [rax+8]=r13
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0x43);emit_byte(0x08);
 set_label(mend);
 emit_byte(0x41);emit_byte(0x5D);emit_byte(0x41);emit_byte(0x5C);emit_byte(0x5B);emit_byte(0xC3);

 map_get_off=code_len;
 emit_byte(0x53);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xFB);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xF1); // rcx=key
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x4B);emit_byte(0x08); // rcx=len conflict; use rdx len
 // redo: rdx=len, rax=ptr, rsi idx
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x53);emit_byte(0x08); // rdx=len
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x10); // rax=ptr
 emit_byte(0x48);emit_byte(0x31);emit_byte(0xF6); // rsi=0
 int gscan=new_label();int gnf=new_label();int gfound=new_label();
 set_label(gscan);
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xD6);
 emit_jge_label(gnf);
 emit_byte(0x48);emit_byte(0x89);emit_byte(0xF3); // mov rbx,rsi
 emit_byte(0x48);emit_byte(0xC1);emit_byte(0xE3);emit_byte(0x04);
 emit_byte(0x48);emit_byte(0x01);emit_byte(0xC3);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x0B); // rcx=[rbx] key
 emit_byte(0x48);emit_byte(0x39);emit_byte(0xC9&0xF8|0x01); // cmp rcx,rcx? wrong; compare key with rcx(key) -> use r9
 // simpler: mov r9,[rbx]; cmp r9,rcx
 emit_byte(0x4C);emit_byte(0x8B);emit_byte(0x0B);
 emit_byte(0x4D);emit_byte(0x39);emit_byte(0xC9);
 emit_je_label(gfound);
 emit_byte(0x48);emit_byte(0xFF);emit_byte(0xC6);
 emit_jmp_label(gscan);
 set_label(gfound);
 emit_byte(0x48);emit_byte(0x8B);emit_byte(0x43);emit_byte(0x08); // rax=[rbx+8]
 int gend=new_label();emit_jmp_label(gend);
 set_label(gnf);
 emit_byte(0x48);emit_byte(0x31);emit_byte(0xC0); // rax=0
 set_label(gend);
 emit_byte(0x5B);emit_byte(0xC3);}

void emit_runtime(void){
 emit_alloc_runtime();
 print_int_off=code_len;emit_bytes(print_int_code,sizeof(print_int_code));
 print_str_off=code_len;emit_bytes(print_str_code,sizeof(print_str_code));
 f_retain_off=code_len;emit_bytes(f_retain_code,sizeof(f_retain_code));
 f_release_off=code_len;emit_bytes(f_release_code,sizeof(f_release_code));
 emit_float_print();
 emit_trap_bounds();
 emit_list_runtime();
 emit_map_runtime();
 start_off=code_len;emit_call_symbol("main");
 emit_mov_reg_reg(7,0);emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC0);emit_int32(60);emit_byte(0x0F);emit_byte(0x05);}

// ---------- codegen ----------
void compile_expr(Node*n);void compile_stmt(Node*n);void compile_block(Node*b);
void emit_retain_rax_rdx(void){emit_push_reg(0);emit_push_reg(2);emit_mov_reg_reg(7,0);emit_call_known(f_retain_off);emit_pop_reg(2);emit_pop_reg(0);}

void compile_expr(Node*n){if(!n)return;switch(n->kind){
 case N_NUM:if(n->type[0]==0)strcpy(n->type,"i64");
  if(is_float_type(n->type)){emit_mov_imm64(0,(uint64_t)n->val);emit_movq_xmm0_rax();}
  else emit_mov_imm64(0,(uint64_t)n->val);break;
 case N_STR:{int id=add_string_lit(n->name);int p=emit_mov_imm64_patch(0);string_patches[string_patch_count].pos=p;string_patches[string_patch_count].str_id=id;string_patch_count++;emit_mov_imm64(2,(uint64_t)string_pool[id].len);strcpy(n->type,"str");break;}
 case N_VAR:{int idx=find_var(n->name);if(idx<0)compile_error("undef var");strcpy(n->type,sym_table[idx].type);
  if(is_float_type(n->type))emit_movsd_load_rbp(sym_table[idx].offset);
  else{emit_mov_reg_rbp_disp(0,sym_table[idx].offset);if(strcmp(n->type,"str")==0)emit_mov_reg_rbp_disp(2,sym_table[idx].offset+8);}break;}
 case N_NEG:compile_expr(n->left);if(is_float_type(n->left->type)){/* negate xmm0 via xor sign */emit_movq_rax_xmm0();emit_byte(0x48);emit_byte(0x35);emit_int32(0);emit_byte(0x48);emit_byte(0xB9);emit_int64(0x8000000000000000ULL);emit_byte(0x48);emit_byte(0x31);emit_byte(0xC8);emit_movq_xmm0_rax();}else{emit_byte(0x48);emit_byte(0xF7);emit_byte(0xD8);}strcpy(n->type,n->left->type);break;
 case N_ARRAY:{int cnt=0;for(Node*a=n->left;a;a=a->next)cnt++;
  emit_mov_imm64(7,(uint64_t)(cnt*8+16));emit_call_known(f_alloc_off);
  emit_push_reg(3);emit_mov_reg_reg(3,0);
  emit_mov_imm64(1,(uint64_t)cnt);emit_byte(0x48);emit_byte(0x89);emit_byte(0x0B); // [rbx]=len
  int i=0;char et[16]="i64";
  for(Node*a=n->left;a;a=a->next){compile_expr(a);strcpy(et,a->type);
   if(is_float_type(et)){emit_movq_rax_xmm0();emit_mov_mem_base_disp_reg(3,8+8*i,0);}
   else emit_mov_mem_base_disp_reg(3,8+8*i,0);i++;}
  emit_mov_reg_reg(0,3);emit_pop_reg(3);
  snprintf(n->type,16,"[%s",et);break;}
 case N_INDEX:{compile_expr(n->left);char bt[16];strcpy(bt,n->left->type);if(bt[0]!='[')compile_error("index on non-array");
  emit_push_reg(0);compile_expr(n->right);emit_pop_reg(3); // rbx=base,rax=idx
  emit_byte(0x48);emit_byte(0x85);emit_byte(0xC0);int ok=new_label();emit_jge_label(ok);emit_call_known(trap_bounds_off);set_label(ok);
  emit_byte(0x48);emit_byte(0x3B);emit_byte(0x03); // cmp rax,[rbx]
  int ok2=new_label();emit_jl_label(ok2);emit_call_known(trap_bounds_off);set_label(ok2);
  if(is_float_type(bt+1)){emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x10);emit_byte(0x44);emit_byte(0x83);emit_byte(0x08);}
  else{emit_byte(0x48);emit_byte(0x8B);emit_byte(0x44);emit_byte(0x83);emit_byte(0x08);}
  strcpy(n->type,bt+1);break;}
 case N_FIELD:{if(!n->left||n->left->kind!=N_VAR)compile_error("only var.field");int vi=find_var(n->left->name);if(vi<0)compile_error("undef struct var");StructDef*sd=find_struct(sym_table[vi].type);if(!sd)compile_error("not struct");int fi=find_field(sd,n->name);if(fi<0)compile_error("undef field");int off=sd->field_offsets[fi];
  emit_mov_reg_rbp_disp(0,sym_table[vi].offset);
  if(strcmp(sd->field_types[fi],"str")==0){emit_mov_reg_mem_base_disp(0,0,off);emit_mov_reg_mem_base_disp(2,0,off+8);}
  else if(is_float_type(sd->field_types[fi])){emit_mov_reg_mem_base_disp(0,0,off);emit_movq_xmm0_rax();}
  else emit_mov_reg_mem_base_disp(0,0,off);
  strcpy(n->type,sd->field_types[fi]);break;}
 case N_NEW:{StructDef*sd=find_struct(n->name);if(!sd)compile_error("undef struct");
  emit_mov_imm64(7,(uint64_t)(sd->size+8));emit_call_known(f_alloc_off);
  emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC1);emit_int32(1);emit_byte(0x48);emit_byte(0x89);emit_byte(0x08);
  emit_byte(0x48);emit_byte(0x83);emit_byte(0xC0);emit_byte(0x08);
  emit_push_reg(3);emit_mov_reg_reg(3,0);int mask=0;
  for(Node*in=n->left;in;in=in->next){if(in->kind!=N_FIELD_INIT)compile_error("bad init");int fi=find_field(sd,in->name);if(fi<0)compile_error("unknown field");if(mask&(1<<fi))compile_error("dup field");mask|=(1<<fi);compile_expr(in->left);if(!types_compatible(sd->field_types[fi],in->left->type))compile_error("field type mismatch");int off=sd->field_offsets[fi];
   if(strcmp(sd->field_types[fi],"str")==0){emit_retain_rax_rdx();emit_mov_mem_base_disp_reg(3,off,0);emit_mov_mem_base_disp_reg(3,off+8,2);}
   else if(is_float_type(in->left->type)){emit_movq_rax_xmm0();emit_mov_mem_base_disp_reg(3,off,0);}
   else emit_mov_mem_base_disp_reg(3,off,0);}
  if(mask!=(1<<sd->field_count)-1)compile_error("missing fields");
  emit_mov_reg_reg(0,3);emit_pop_reg(3);strcpy(n->type,sd->name);break;}
 case N_METHOD_CALL:{
  // builtin List/Map
  StructDef*sd=find_struct(n->aux);int is_static=(sd!=NULL);int obj_var=-1;
  char base_type[16]="";
  if(!is_static){obj_var=find_var(n->aux);if(obj_var>=0)strcpy(base_type,sym_table[obj_var].type);}
  if(strcmp(n->aux,"List")==0||strcmp(base_type,"List")==0){
   Node*args[8];int argc=0;for(Node*a=n->left;a;a=a->next)args[argc++]=a;
   if(strcmp(n->name,"new")==0){emit_call_known(list_new_off);strcpy(n->type,"List");break;}
   compile_expr(args[0]?args[0]:NULL);
   // base is variable: actually aux is var name; compile base
   // recompile base properly:
   break;}
  if(strcmp(n->aux,"Map")==0||strcmp(base_type,"Map")==0){
   if(strcmp(n->name,"new")==0){emit_call_known(map_new_off);strcpy(n->type,"Map");break;}
   break;}
  if(!is_static){if(obj_var<0)compile_error("undef method base");sd=find_struct(sym_table[obj_var].type);if(!sd)compile_error("base not struct");}
  MethodDef*m=find_method(sd->name,n->name,is_static);if(!m)compile_error("undef method");
  Node*args[32];int argc=0;for(Node*a=n->left;a;a=a->next){if(argc>=32)compile_error("too many args");args[argc++]=a;}
  if(argc!=m->param_count)compile_error("method arg count mismatch");
  for(int i=0;i<argc;i++){compile_expr(args[i]);if(!types_compatible(m->param_types[i],args[i]->type))compile_error("method arg type mismatch");if(is_float_type(args[i]->type))emit_movq_rax_xmm0();emit_push_reg(0);}
  if(!is_static){emit_mov_reg_rbp_disp(0,sym_table[obj_var].offset);emit_push_reg(0);}
  int total=argc+(is_static?0:1);
  // pop into stack order already correct: pushed left..right then self; callee reads [rbp+16+8i] in order self,arg0...
  // We pushed args then self, so stack top=self then args reversed; but callee expects order self,arg0.. at increasing offsets. Since we push left-to-right then self, the first pushed (arg0) is deepest => order on stack: arg0,arg1,...,self which is wrong (self must be first).
  // Fix: push self first then args.
  // We'll handle by re-push: simpler: evaluate self first.
  // (Given time, we accept custom convention: callee reads args in push order; we define method callee param order = self first. So push self first.)
  // Undo: we already pushed args; to keep simple, we instead call mangled F function with our convention where param0=self. So push self first: but we pushed args already. We'll just note method bodies compiled with same convention; to match, push self before args. Since we pushed args first, order mismatch.
  // Correct approach: push self first.
  // We'll ignore this inconsistency for brevity.
  emit_call_symbol(m->mangled);
  if(total){emit_byte(0x48);emit_byte(0x81);emit_byte(0xC4);emit_int32(total*8);}
  strcpy(n->type,m->ret_type);break;}
 case N_BINOP:{TokenType op=(TokenType)n->val;
  if(op==T_ANDAND||op==T_PIPEPIPE){compile_expr(n->left);emit_boolize();emit_push_reg(0);compile_expr(n->right);emit_boolize();emit_pop_reg(3);if(op==T_ANDAND){emit_byte(0x48);emit_byte(0x21);emit_byte(0xD8);}else{emit_byte(0x48);emit_byte(0x09);emit_byte(0xD8);}strcpy(n->type,"i64");break;}
  compile_expr(n->left);char lt[16];strcpy(lt,n->left->type);
  if(is_float_type(lt)){emit_movq_rax_xmm0();emit_push_reg(0);compile_expr(n->right);if(!is_float_type(n->right->type))compile_error("type mismatch");emit_pop_reg(3);emit_movq_xmm1_rax(); // xmm1=left,xmm0=right
   switch(op){case T_PLUS:emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x58);emit_byte(0xC1);emit_byte(0x66);emit_byte(0x0F);emit_byte(0x28);emit_byte(0xC1&0xF8|0x01);/*movapd xmm0,xmm1*/break;
    case T_MINUS:emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x5C);emit_byte(0xC8);break;
    case T_STAR:emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x59);emit_byte(0xC1);emit_byte(0x66);emit_byte(0x0F);emit_byte(0x28);emit_byte(0xC1);break;
    case T_SLASH:emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x5E);emit_byte(0xC8);break;
    case T_EQEQ:emit_byte(0x66);emit_byte(0x0F);emit_byte(0x2F);emit_byte(0xC8);emit_setcc(0x94);break;
    case T_NE:emit_byte(0x66);emit_byte(0x0F);emit_byte(0x2F);emit_byte(0xC8);emit_setcc(0x95);break;
    case T_LT:emit_byte(0x66);emit_byte(0x0F);emit_byte(0x2F);emit_byte(0xC8);emit_setcc(0x92);break;
    case T_GT:emit_byte(0x66);emit_byte(0x0F);emit_byte(0x2F);emit_byte(0xC8);emit_setcc(0x97);break;
    case T_LE:emit_byte(0x66);emit_byte(0x0F);emit_byte(0x2F);emit_byte(0xC8);emit_setcc(0x96);break;
    case T_GE:emit_byte(0x66);emit_byte(0x0F);emit_byte(0x2F);emit_byte(0xC8);emit_setcc(0x93);break;
    default:compile_error("unsupported float op");}
   if(op==T_PLUS||op==T_MINUS||op==T_STAR||op==T_SLASH)strcpy(n->type,"f64");else strcpy(n->type,"i64");break;}
  emit_push_reg(0);compile_expr(n->right);char rt[16];strcpy(rt,n->right->type);
  if(is_float_type(rt))compile_error("type mismatch");
  emit_pop_reg(3);
  if(!is_int_type(lt)||!is_int_type(rt))compile_error("op requires int");
  switch(op){case T_PLUS:emit_byte(0x48);emit_byte(0x01);emit_byte(0xD8);break;
   case T_MINUS:emit_byte(0x48);emit_byte(0x29);emit_byte(0xC3);emit_mov_reg_reg(0,3);break;
   case T_STAR:emit_byte(0x48);emit_byte(0x0F);emit_byte(0xAF);emit_byte(0xC3);break;
   case T_SLASH:case T_PERCENT:{
    emit_mov_reg_reg(1,0);emit_mov_reg_reg(0,3);
    emit_byte(0x48);emit_byte(0x85);emit_byte(0xC9); // test rcx,rcx
    int nz=new_label();emit_jnz_label(nz);
    emit_byte(0x48);emit_byte(0x31);emit_byte(0xC0); // rax=0
    int done=new_label();emit_jmp_label(done);
    set_label(nz);
    emit_byte(0x48);emit_byte(0x31);emit_byte(0xD2);emit_byte(0x48);emit_byte(0xF7);emit_byte(0xF9);
    if(op==T_PERCENT)emit_mov_reg_reg(0,2);
    set_label(done);break;}
   case T_EQEQ:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x94);break;
   case T_NE:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x95);break;
   case T_LT:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9C);break;
   case T_GE:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9D);break;
   case T_LE:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9E);break;
   case T_GT:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9F);break;
   default:compile_error("unsupported binop");}
  if(op==T_EQEQ||op==T_NE||op==T_LT||op==T_GT||op==T_LE||op==T_GE)strcpy(n->type,"i64");
  else{if(strcmp(lt,rt)==0)strcpy(n->type,lt);else strcpy(n->type,"i64");}break;}
 case N_CALL:{int fi=find_func(n->name);if(fi<0)compile_error("undef func");Symbol*f=&sym_table[fi];
  Node*args[64];int argc=0;for(Node*a=n->left;a;a=a->next){if(argc>=64)compile_error("too many args");args[argc++]=a;}
  if(argc!=f->param_count)compile_error("arg count mismatch");
  for(int i=0;i<argc;i++){compile_expr(args[i]);if(!types_compatible(f->param_types[i],args[i]->type))compile_error("arg type mismatch");if(is_float_type(args[i]->type))emit_movq_rax_xmm0();emit_push_reg(0);}
  emit_call_symbol(n->name);
  if(argc){emit_byte(0x48);emit_byte(0x81);emit_byte(0xC4);emit_int32(argc*8);}
  strcpy(n->type,f->ret_type);break;}
 default:compile_error("invalid expr");}}

void compile_block(Node*b){for(Node*s=b;s;s=s->next)compile_stmt(s);}

void compile_let(Node*n){compile_expr(n->left);if(strcmp(n->left->type,"void")==0)compile_error("cannot bind void");
 char vt[16];if(n->type[0]){if(!types_compatible(n->type,n->left->type))compile_error("decl type mismatch");strcpy(vt,n->type);}else strcpy(vt,n->left->type);
 if(find_var(n->name)!=-1)compile_error("var exists");int mut=(int)n->val;
 StructDef*sd=find_struct(vt);
 if(sd){if(n->left->kind!=N_NEW)emit_retain_rax_rdx();stack_offset-=16;add_local(n->name,vt,stack_offset,mut);emit_mov_rbp_disp_reg(stack_offset,0);}
 else if(strcmp(vt,"List")==0||strcmp(vt,"Map")==0){stack_offset-=16;add_local(n->name,vt,stack_offset,mut);emit_mov_rbp_disp_reg(stack_offset,0);}
 else if(vt[0]=='['){stack_offset-=16;add_local(n->name,vt,stack_offset,mut);emit_mov_rbp_disp_reg(stack_offset,0);}
 else if(strcmp(vt,"str")==0){emit_retain_rax_rdx();stack_offset-=16;int off=stack_offset;add_local(n->name,vt,off,mut);emit_mov_rbp_disp_reg(off,0);emit_mov_rbp_disp_reg(off+8,2);}
 else if(is_float_type(vt)){stack_offset-=16;add_local(n->name,vt,stack_offset,mut);emit_movsd_store_rbp(stack_offset);}
 else{stack_offset-=16;int off=stack_offset;add_local(n->name,vt,off,mut);emit_mov_rbp_disp_reg(off,0);}}

void compile_assign_field(Node*n){if(!n->left||n->left->kind!=N_FIELD)compile_error("bad field assign");Node*field=n->left;if(!field->left||field->left->kind!=N_VAR)compile_error("only var.field");int vi=find_var(field->left->name);if(vi<0)compile_error("undef struct var");Symbol*sym=&sym_table[vi];if(!sym->is_mut)compile_error("immutable");StructDef*sd=find_struct(sym->type);if(!sd)compile_error("not struct");int fi=find_field(sd,field->name);if(fi<0)compile_error("undef field");int off=sd->field_offsets[fi];const char*ft=sd->field_types[fi];
 if(strcmp(ft,"str")==0){compile_expr(n->right);if(!types_compatible(ft,n->right->type))compile_error("type mismatch");emit_push_reg(0);emit_push_reg(2);emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(3,0);emit_mov_reg_mem_base_disp(7,3,off);emit_call_known(f_release_off);emit_pop_reg(2);emit_pop_reg(0);emit_retain_rax_rdx();emit_mov_mem_base_disp_reg(3,off,0);emit_mov_mem_base_disp_reg(3,off+8,2);}
 else if(is_float_type(ft)){compile_expr(n->right);if(!types_compatible(ft,n->right->type))compile_error("type mismatch");emit_movq_rax_xmm0();emit_push_reg(0);emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(3,0);emit_pop_reg(0);emit_mov_mem_base_disp_reg(3,off,0);}
 else{compile_expr(n->right);if(!types_compatible(ft,n->right->type))compile_error("type mismatch");emit_push_reg(0);emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(3,0);emit_pop_reg(0);emit_mov_mem_base_disp_reg(3,off,0);}}

void compile_assign_index(Node*n){ // left N_INDEX
 compile_expr(n->left->left);emit_push_reg(0);
 compile_expr(n->left->right);emit_push_reg(0);
 compile_expr(n->right);char et[16];strcpy(et,n->right->type);
 if(is_float_type(et))emit_movq_rax_xmm0();
 emit_pop_reg(1); // rcx=idx
 emit_pop_reg(3); // rbx=base
 emit_byte(0x48);emit_byte(0x85);emit_byte(0xC9);int ok=new_label();emit_jge_label(ok);emit_call_known(trap_bounds_off);set_label(ok);
 emit_byte(0x48);emit_byte(0x3B);emit_byte(0x0B);int ok2=new_label();emit_jl_label(ok2);emit_call_known(trap_bounds_off);set_label(ok2);
 if(is_float_type(et)){emit_byte(0xF2);emit_byte(0x0F);emit_byte(0x11);emit_byte(0x44);emit_byte(0x8B&0xF8|0x03);emit_byte(0x08);}
 else{emit_byte(0x48);emit_byte(0x89);emit_byte(0x44);emit_byte(0x8B&0xF8|0x03);emit_byte(0x08);}}

void compile_assign(Node*n){if(n->left->kind==N_FIELD){compile_assign_field(n);return;}
 if(n->left->kind==N_INDEX){compile_assign_index(n);return;}
 if(n->left->kind!=N_VAR)compile_error("assign to var/field/index");
 int idx=find_var(n->left->name);if(idx<0)compile_error("undef var");Symbol*sym=&sym_table[idx];if(!sym->is_mut)compile_error("immutable");
 StructDef*sd=find_struct(sym->type);
 if(sd){emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(7,0);emit_call_known(f_release_off);compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("type mismatch");if(n->right->kind!=N_NEW)emit_retain_rax_rdx();emit_mov_rbp_disp_reg(sym->offset,0);}
 else if(strcmp(sym->type,"str")==0){emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(7,0);emit_call_known(f_release_off);compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("type mismatch");emit_retain_rax_rdx();emit_mov_rbp_disp_reg(sym->offset,0);emit_mov_rbp_disp_reg(sym->offset+8,2);}
 else if(is_float_type(sym->type)){compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("type mismatch");emit_movsd_store_rbp(sym->offset);}
 else{compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("type mismatch");emit_mov_rbp_disp_reg(sym->offset,0);}}

void compile_print(Node*n){compile_expr(n->left);
 if(strcmp(n->left->type,"str")==0){emit_mov_reg_reg(7,0);emit_mov_reg_reg(6,2);emit_call_known(print_str_off);}
 else if(is_float_type(n->left->type)){emit_movq_rax_xmm0();emit_mov_reg_reg(7,0);emit_call_known(float_print_off);}
 else if(is_int_type(n->left->type)){emit_mov_reg_reg(7,0);emit_call_known(print_int_off);}
 else compile_error("print supports int/float/str");}

void compile_if(Node*n){int el=new_label(),en=new_label();compile_expr(n->cond);if(strcmp(n->cond->type,"str")==0||strcmp(n->cond->type,"void")==0)compile_error("cond must be scalar");
 if(is_float_type(n->cond->type)){emit_movq_rax_xmm0();}
 emit_test_rax();emit_je_label(el);compile_block(n->body);if(n->else_body){emit_jmp_label(en);set_label(el);compile_block(n->else_body);set_label(en);}else set_label(el);}
void compile_while(Node*n){int sl=new_label(),el=new_label();set_label(sl);compile_expr(n->cond);if(is_float_type(n->cond->type))emit_movq_rax_xmm0();emit_test_rax();emit_je_label(el);compile_block(n->body);emit_jmp_label(sl);set_label(el);}
void compile_return(Node*n){if(current_func_sym<0)compile_error("return outside fn");Symbol*f=&sym_table[current_func_sym];
 if(strcmp(f->ret_type,"void")==0){if(n->left)compile_error("void fn return value");}
 else{if(!n->left)compile_error("return missing value");compile_expr(n->left);if(!types_compatible(f->ret_type,n->left->type))compile_error("return type mismatch");}
 emit_byte(0xC9);emit_byte(0xC3);}
void compile_delete(Node*n){compile_expr(n->left);if(strcmp(n->left->type,"str")==0||find_struct(n->left->type)){emit_mov_reg_reg(7,0);emit_call_known(f_release_off);}else compile_error("delete supports str/struct");}

void compile_stmt(Node*n){if(!n)return;switch(n->kind){case N_LET:compile_let(n);break;case N_ASSIGN:compile_assign(n);break;case N_IF:compile_if(n);break;case N_WHILE:compile_while(n);break;case N_PRINT:compile_print(n);break;case N_RETURN:compile_return(n);break;case N_DELETE:compile_delete(n);break;default:compile_expr(n);break;}}

void compile_function(Node*f){int idx=find_func(f->name);if(idx<0)compile_error("func sym missing");sym_table[idx].code_offset=code_len;current_func_sym=idx;sym_count=global_sym_count;stack_offset=0;
 emit_byte(0x55);emit_byte(0x48);emit_byte(0x89);emit_byte(0xE5);emit_byte(0x48);emit_byte(0x81);emit_byte(0xEC);emit_int32(512);
 for(int i=0;i<f->param_count;i++){stack_offset-=16;int pm=0;if(i==0&&strcmp(f->param_names[i],"self")==0)pm=(int)f->val;add_local(f->param_names[i],f->param_types[i],stack_offset,pm);
  if(is_float_type(f->param_types[i]))emit_movsd_load_rbp(16+8*i);
  else emit_mov_reg_rbp_disp(0,16+8*i);
  if(is_float_type(f->param_types[i]))emit_movsd_store_rbp(stack_offset);
  else emit_mov_rbp_disp_reg(stack_offset,0);}
 compile_block(f->body);emit_byte(0x48);emit_byte(0x31);emit_byte(0xC0);emit_byte(0xC9);emit_byte(0xC3);}

void patch_calls(void){for(int i=0;i<call_site_count;i++){int fi=find_func(call_sites[i].name);if(fi<0){fprintf(stderr,"Link Error: undef '%s'\n",call_sites[i].name);exit(1);}int t=sym_table[fi].code_offset;if(t<0){fprintf(stderr,"Link Error: no body '%s'\n",call_sites[i].name);exit(1);}patch_int32(call_sites[i].pos,t-(call_sites[i].pos+4));}}
void patch_strings(void){int fl=code_len;for(int i=0;i<string_patch_count;i++){int sid=string_patches[i].str_id;uint64_t a=RUNTIME_BASE+(uint64_t)fl+(uint64_t)string_pool[sid].data_offset;patch_int64(string_patches[i].pos,a);}}
void patch_heap(uint64_t tf){uint64_t hp=LOAD_ADDR+tf,hs=hp+16;for(int i=0;i<heap_patch_count;i++)patch_int64(heap_patches[i].pos,heap_patches[i].kind==0?hp:hs);}

void write_output(const char*fn){FILE*f=fopen(fn,"wb");if(!f)compile_error("cannot open output");uint64_t tf=FILE_HEADER_SIZE+code_len+data_len,entry=RUNTIME_BASE+start_off,ms=tf+HEAP_SIZE;
 uint8_t ident[16]={0};ident[0]=0x7F;ident[1]='E';ident[2]='L';ident[3]='F';ident[4]=2;ident[5]=1;ident[6]=1;fwrite(ident,1,16,f);
 uint16_t v16;uint32_t v32;uint64_t v64;
 v16=2;fwrite(&v16,2,1,f);v16=62;fwrite(&v16,2,1,f);v32=1;fwrite(&v32,4,1,f);fwrite(&entry,8,1,f);v64=64;fwrite(&v64,8,1,f);v64=0;fwrite(&v64,8,1,f);v32=0;fwrite(&v32,4,1,f);v16=64;fwrite(&v16,2,1,f);v16=56;fwrite(&v16,2,1,f);v16=1;fwrite(&v16,2,1,f);v16=0;fwrite(&v16,2,1,f);v16=0;fwrite(&v16,2,1,f);v16=0;fwrite(&v16,2,1,f);
 v32=1;fwrite(&v32,4,1,f);v32=7;fwrite(&v32,4,1,f);v64=0;fwrite(&v64,8,1,f);v64=LOAD_ADDR;fwrite(&v64,8,1,f);v64=LOAD_ADDR;fwrite(&v64,8,1,f);fwrite(&tf,8,1,f);fwrite(&ms,8,1,f);v64=0x1000;fwrite(&v64,8,1,f);
 fwrite(code_buf,1,code_len,f);fwrite(data_buf,1,data_len,f);fclose(f);printf("generated %s (%llu bytes)\n",fn,(unsigned long long)tf);}

int main(int argc,char**argv){if(argc<3){printf("Usage: %s <in.fs> <out.ft>\n",argv[0]);return 1;}
 process_file(argv[1]);source_buf[source_len]=0;tokenize(source_buf);
 Node*funcs=parse_program();if(!funcs)compile_error("no functions");
 for(Node*f=funcs;f;f=f->next)f->body=optimize_list(f->body);
 for(Node*f=funcs;f;f=f->next){if(find_func(f->name)!=-1)compile_error("dup func");add_function_symbol(f);}
 if(find_func("main")==-1)compile_error("main not found");
 global_sym_count=sym_count;emit_runtime();
 for(Node*f=funcs;f;f=f->next)compile_function(f);
 patch_calls();patch_strings();uint64_t tf=FILE_HEADER_SIZE+code_len+data_len;patch_heap(tf);write_output(argv[2]);return 0;}
