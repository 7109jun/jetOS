#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

// ============================================================
// F Compiler v6 Ultimate
// Pure C, zero external dependencies
//
// Features:
// - functions, let/mut, if/else, while
// - print(int), print(str)
// - string literals with RC header
// - struct, new/delete, field access
// - impl blocks, instance methods (self/mut self), static methods
// - simple bump allocator f_alloc
// - import module textual inclusion
// - AST constant folding optimization
// - custom .ft output (ELF-based native executable)
// ============================================================

#define MAX_TOKENS 8192
#define MAX_NODES 4096
#define MAX_SYMS 256
#define MAX_STRUCTS 64
#define MAX_FIELDS 8
#define MAX_METHODS 256
#define CODE_SIZE 131072
#define DATA_SIZE 16384
#define MAX_PATCHES 1024
#define MAX_CALL_SITES 512
#define MAX_STRING_PATCHES 512
#define MAX_STRINGS 256
#define MAX_HEAP_PATCHES 64

#define SRC_BUF_SIZE (1 << 20)
#define MAX_VISITED_FILES 32

#define LOAD_ADDR 0x400000ULL
#define FILE_HEADER_SIZE 120
#define RUNTIME_BASE (LOAD_ADDR + FILE_HEADER_SIZE)
#define HEAP_SIZE (1 << 20)

typedef enum {
    T_INVALID = 0,
    T_EOF,
    T_LET, T_MUT, T_FN, T_IF, T_ELSE, T_WHILE, T_RETURN, T_PRINT,
    T_TRUE, T_FALSE,
    T_STRUCT, T_NEW, T_DELETE, T_IMPORT, T_IMPL, T_SELF, T_STATIC,
    T_IDENT, T_INT, T_STR,
    T_ASSIGN, T_EQEQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_ANDAND, T_PIPEPIPE,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE,
    T_SEMI, T_COMMA, T_COLON, T_ARROW, T_DOT
} TokenType;

typedef struct {
    TokenType type;
    char text[256];
    int64_t int_val;
} Token;

typedef enum {
    N_NUM, N_STR, N_VAR, N_BINOP, N_NEG, N_ASSIGN, N_LET,
    N_IF, N_WHILE, N_PRINT, N_RETURN, N_FUNC, N_CALL,
    N_FIELD, N_NEW, N_DELETE, N_FIELD_INIT, N_METHOD_CALL
} NodeKind;

typedef struct Node {
    NodeKind kind;
    int64_t val;
    char name[64];
    char aux[64]; // method base (var name or struct name) or temp storage
    char type[16];
    char ret_type[16];
    int param_count;
    char param_names[3][64];
    char param_types[3][16];
    struct Node *left, *right, *cond, *body, *else_body, *next;
} Node;

typedef struct {
    char name[64];
    int is_func;
    int code_offset;
    int offset;
    int is_mut;
    char type[16];
    int param_count;
    char param_types[3][16];
    char ret_type[16];
} Symbol;

typedef struct {
    char name[64];
    int field_count;
    char field_names[MAX_FIELDS][64];
    char field_types[MAX_FIELDS][16];
    int field_offsets[MAX_FIELDS];
    int size;
} StructDef;

typedef struct {
    char struct_name[64];
    char method_name[64];
    char mangled[64];
    int is_static;
    int param_count; // excluding self
    char param_types[3][16];
    char ret_type[16];
} MethodDef;

typedef struct { int pos; int target; } Patch;
typedef struct { int pos; char name[64]; } CallSite;
typedef struct { int pos; int str_id; } StringPatch;
typedef struct { char text[256]; int len; int data_offset; } StringLit;
typedef struct { int pos; int kind; } HeapPatch; // 0=heap_ptr, 1=heap_start

Token tokens[MAX_TOKENS];
int token_count = 0, pos = 0;

Node node_pool[MAX_NODES];
int node_count = 0;

Symbol sym_table[MAX_SYMS];
int sym_count = 0, global_sym_count = 0, current_func_sym = -1;

StructDef structs[MAX_STRUCTS];
int struct_count = 0;

MethodDef methods[MAX_METHODS];
int method_count = 0;

uint8_t code_buf[CODE_SIZE];
int code_len = 0;

uint8_t data_buf[DATA_SIZE];
int data_len = 0;

Patch patches[MAX_PATCHES];
int patch_count = 0;

int label_pos[MAX_PATCHES];
int label_count = 0;

CallSite call_sites[MAX_CALL_SITES];
int call_site_count = 0;

StringPatch string_patches[MAX_STRING_PATCHES];
int string_patch_count = 0;

StringLit string_pool[MAX_STRINGS];
int string_pool_count = 0;

HeapPatch heap_patches[MAX_HEAP_PATCHES];
int heap_patch_count = 0;

int f_alloc_off = 0;
int print_int_off = 0, print_str_off = 0;
int f_retain_off = 0, f_release_off = 0;
int start_off = 0;

int stack_offset = 0;

char source_buf[SRC_BUF_SIZE];
int source_len = 0;

char visited_files[MAX_VISITED_FILES][256];
int visited_file_count = 0;

// ------------------------------------------------------------
// Utilities
// ------------------------------------------------------------
void compile_error(const char *msg) {
    fprintf(stderr, "F Compiler Error: %s\n", msg);
    exit(1);
}

Node *new_node(NodeKind kind) {
    if (node_count >= MAX_NODES) compile_error("AST overflow");
    Node *n = &node_pool[node_count++];
    memset(n, 0, sizeof(Node));
    n->kind = kind;
    return n;
}

int is_int_type(const char *t) {
    return strcmp(t, "i32") == 0 || strcmp(t, "i64") == 0 ||
           strcmp(t, "u32") == 0 || strcmp(t, "u64") == 0 ||
           strcmp(t, "bool") == 0;
}

int types_compatible(const char *a, const char *b) {
    if (strcmp(a, b) == 0) return 1;
    if (is_int_type(a) && is_int_type(b)) return 1;
    return 0;
}

StructDef *find_struct(const char *name) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(structs[i].name, name) == 0) return &structs[i];
    }
    return NULL;
}

int find_field(StructDef *s, const char *name) {
    for (int i = 0; i < s->field_count; i++) {
        if (strcmp(s->field_names[i], name) == 0) return i;
    }
    return -1;
}

MethodDef *find_method(const char *sname, const char *mname, int is_static) {
    for (int i = 0; i < method_count; i++) {
        if (strcmp(methods[i].struct_name, sname) == 0 &&
            strcmp(methods[i].method_name, mname) == 0 &&
            methods[i].is_static == is_static) {
            return &methods[i];
        }
    }
    return NULL;
}

// ------------------------------------------------------------
// Import preprocessor
// ------------------------------------------------------------
void append_src(const char *s, int len) {
    if (source_len + len >= SRC_BUF_SIZE) compile_error("source buffer overflow");
    memcpy(source_buf + source_len, s, len);
    source_len += len;
}

char *read_whole_file(const char *path, long *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "Cannot open file: %s\n", path); exit(1); }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    if (!buf) compile_error("out of memory");
    fread(buf, 1, len, fp);
    buf[len] = '\0';
    fclose(fp);
    if (out_len) *out_len = len;
    return buf;
}

int file_visited(const char *path) {
    for (int i = 0; i < visited_file_count; i++)
        if (strcmp(visited_files[i], path) == 0) return 1;
    return 0;
}

void process_file(const char *path);

void process_import_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "import", 6) != 0) return;
    p += 6;
    while (*p == ' ' || *p == '\t') p++;
    char mod[256]; int mi = 0;
    while (isalnum((unsigned char)*p) || *p == '_') {
        if (mi < 255) mod[mi++] = *p; p++;
    }
    mod[mi] = '\0';
    if (mi == 0) return;
    char fpath[512]; snprintf(fpath, sizeof(fpath), "%s.fs", mod);
    process_file(fpath);
}

void process_file(const char *path) {
    if (file_visited(path)) return;
    if (visited_file_count >= MAX_VISITED_FILES) compile_error("too many imported files");
    strcpy(visited_files[visited_file_count++], path);
    long len; char *txt = read_whole_file(path, &len);
    char *p = txt, *end = txt + len;
    while (p < end) {
        char *ls = p;
        while (p < end && *p != '\n') p++;
        int ll = (int)(p - ls); int nl = (p < end);
        char tmp[1024]; int cl = ll < 1023 ? ll : 1023;
        memcpy(tmp, ls, cl); tmp[cl] = '\0';
        char *t = tmp; while (*t == ' ' || *t == '\t') t++;
        if (strncmp(t, "import", 6) == 0 && (t[6] == ' ' || t[6] == '\t')) {
            process_import_line(t);
        } else {
            append_src(ls, ll); if (nl) append_src("\n", 1);
        }
        if (nl) p++;
    }
    free(txt);
}

// ------------------------------------------------------------
// Lexer
// ------------------------------------------------------------
Token peek(void) { return tokens[pos]; }
Token advance(void) { return tokens[pos++]; }
int check(TokenType t) { return peek().type == t; }

void expect(TokenType t) {
    if (!check(t)) { fprintf(stderr, "Parser Error: expected %d, got %d\n", t, peek().type); exit(1); }
    advance();
}

Token expect_ident(void) {
    if (!check(T_IDENT)) compile_error("expected identifier");
    return advance();
}

void tokenize(const char *src) {
    int i = 0;
    while (src[i]) {
        while (isspace((unsigned char)src[i])) i++;
        if (src[i] == '/' && src[i+1] == '/') { while (src[i] && src[i]!='\n') i++; continue; }
        if (src[i] == '/' && src[i+1] == '*') { i+=2; while (src[i] && !(src[i]=='*' && src[i+1]=='/')) i++; if(src[i]) i+=2; continue; }
        if (!src[i]) break;
        Token t; memset(&t, 0, sizeof(t));
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            int j = 0;
            while (isalnum((unsigned char)src[i]) || src[i] == '_') { if(j<255) t.text[j++]=src[i]; i++; }
            t.text[j] = '\0';
            if (strcmp(t.text,"let")==0) t.type=T_LET;
            else if (strcmp(t.text,"mut")==0) t.type=T_MUT;
            else if (strcmp(t.text,"fn")==0) t.type=T_FN;
            else if (strcmp(t.text,"if")==0) t.type=T_IF;
            else if (strcmp(t.text,"else")==0) t.type=T_ELSE;
            else if (strcmp(t.text,"while")==0) t.type=T_WHILE;
            else if (strcmp(t.text,"return")==0) t.type=T_RETURN;
            else if (strcmp(t.text,"print")==0) t.type=T_PRINT;
            else if (strcmp(t.text,"true")==0) t.type=T_TRUE;
            else if (strcmp(t.text,"false")==0) t.type=T_FALSE;
            else if (strcmp(t.text,"struct")==0) t.type=T_STRUCT;
            else if (strcmp(t.text,"new")==0) t.type=T_NEW;
            else if (strcmp(t.text,"delete")==0) t.type=T_DELETE;
            else if (strcmp(t.text,"import")==0) t.type=T_IMPORT;
            else if (strcmp(t.text,"impl")==0) t.type=T_IMPL;
            else if (strcmp(t.text,"self")==0) t.type=T_SELF;
            else if (strcmp(t.text,"static")==0) t.type=T_STATIC;
            else t.type=T_IDENT;
        } else if (isdigit((unsigned char)src[i])) {
            int j=0; while(isdigit((unsigned char)src[i])){if(j<255)t.text[j++]=src[i];i++;} t.text[j]='\0';
            t.int_val=atoll(t.text); t.type=T_INT;
        } else if (src[i]=='"') {
            i++; int j=0;
            while(src[i] && src[i]!='"'){
                if(src[i]=='\\' && src[i+1]){i++;
                    if(src[i]=='n'){if(j<255)t.text[j++]='\n';}
                    else if(src[i]=='t'){if(j<255)t.text[j++]='\t';}
                    else if(src[i]=='\\'){if(j<255)t.text[j++]='\\';}
                    else if(src[i]=='"'){if(j<255)t.text[j++]='"';}
                    else{if(j<255)t.text[j++]=src[i];}
                } else { if(j<255)t.text[j++]=src[i]; }
                i++;
            }
            if(src[i]=='"') i++; t.text[j]='\0'; t.type=T_STR;
        } else {
            switch(src[i]){
                case '=': if(src[i+1]=='='){t.type=T_EQEQ;i+=2;}else{t.type=T_ASSIGN;i++;} break;
                case '!': if(src[i+1]=='='){t.type=T_NE;i+=2;}else compile_error("unexpected '!'"); break;
                case '<': if(src[i+1]=='='){t.type=T_LE;i+=2;}else{t.type=T_LT;i++;} break;
                case '>': if(src[i+1]=='='){t.type=T_GE;i+=2;}else{t.type=T_GT;i++;} break;
                case '&': if(src[i+1]=='&'){t.type=T_ANDAND;i+=2;}else compile_error("single '&' not supported"); break;
                case '|': if(src[i+1]=='|'){t.type=T_PIPEPIPE;i+=2;}else compile_error("single '|' not supported"); break;
                case '-': if(src[i+1]=='>'){t.type=T_ARROW;i+=2;}else{t.type=T_MINUS;i++;} break;
                case '+': t.type=T_PLUS; i++; break;
                case '*': t.type=T_STAR; i++; break;
                case '/': t.type=T_SLASH; i++; break;
                case '%': t.type=T_PERCENT; i++; break;
                case '(': t.type=T_LPAREN; i++; break;
                case ')': t.type=T_RPAREN; i++; break;
                case '{': t.type=T_LBRACE; i++; break;
                case '}': t.type=T_RBRACE; i++; break;
                case ';': t.type=T_SEMI; i++; break;
                case ',': t.type=T_COMMA; i++; break;
                case ':': t.type=T_COLON; i++; break;
                case '.': t.type=T_DOT; i++; break;
                default: fprintf(stderr,"Lexer Error: '%c'\n",src[i]); exit(1);
            }
        }
        if(token_count>=MAX_TOKENS) compile_error("token overflow");
        tokens[token_count++]=t;
    }
    tokens[token_count].type=T_EOF;
}

// ------------------------------------------------------------
// Parser
// ------------------------------------------------------------
void parse_type_into(char out[16]) {
    Token t=peek(); if(t.type!=T_IDENT) compile_error("expected type"); advance();
    strncpy(out,t.text,15); out[15]='\0';
}

Node *parse_expr(void);
Node *parse_stmt(void);
Node *parse_block(void);

Node *parse_primary(void) {
    Token t = peek();

    if (t.type == T_INT) { advance(); Node*n=new_node(N_NUM); n->val=t.int_val; return n; }
    if (t.type == T_TRUE) { advance(); Node*n=new_node(N_NUM); n->val=1; strcpy(n->type,"bool"); return n; }
    if (t.type == T_FALSE) { advance(); Node*n=new_node(N_NUM); n->val=0; strcpy(n->type,"bool"); return n; }
    if (t.type == T_STR) { advance(); Node*n=new_node(N_STR); strcpy(n->name,t.text); return n; }

    if (t.type == T_NEW) {
        advance(); Node*n=new_node(N_NEW); Token tn=expect_ident(); strcpy(n->name,tn.text);
        expect(T_LBRACE); Node*head=NULL,*tail=NULL;
        while(!check(T_RBRACE)){
            Node*init=new_node(N_FIELD_INIT); Token fn=expect_ident(); strcpy(init->name,fn.text);
            expect(T_COLON); init->left=parse_expr();
            if(!head)head=init; else tail->next=init; tail=init;
            if(check(T_COMMA))advance(); else break;
        }
        expect(T_RBRACE); n->left=head; return n;
    }

    if (t.type == T_SELF) {
        advance();
        if (check(T_DOT)) {
            advance(); Token member=expect_ident();
            if (check(T_LPAREN)) {
                advance(); Node*call=new_node(N_METHOD_CALL);
                strcpy(call->name,member.text); strcpy(call->aux,"self");
                Node*head=NULL,*tail=NULL;
                while(!check(T_RPAREN)){ Node*arg=parse_expr(); if(!head)head=arg;else tail->next=arg;tail=arg; if(check(T_COMMA))advance();else break; }
                expect(T_RPAREN); call->left=head; return call;
            }
            Node*base=new_node(N_VAR); strcpy(base->name,"self");
            Node*f=new_node(N_FIELD); f->left=base; strcpy(f->name,member.text); return f;
        }
        Node*n=new_node(N_VAR); strcpy(n->name,"self"); return n;
    }

    if (t.type == T_IDENT) {
        advance();
        if (check(T_LPAREN)) {
            advance(); Node*call=new_node(N_CALL); strcpy(call->name,t.text);
            Node*head=NULL,*tail=NULL;
            while(!check(T_RPAREN)){ Node*arg=parse_expr(); if(!head)head=arg;else tail->next=arg;tail=arg; if(check(T_COMMA))advance();else break; }
            expect(T_RPAREN); call->left=head; return call;
        }
        if (check(T_DOT)) {
            advance(); Token member=expect_ident();
            if (check(T_LPAREN)) {
                advance(); Node*call=new_node(N_METHOD_CALL);
                strcpy(call->name,member.text); strcpy(call->aux,t.text);
                Node*head=NULL,*tail=NULL;
                while(!check(T_RPAREN)){ Node*arg=parse_expr(); if(!head)head=arg;else tail->next=arg;tail=arg; if(check(T_COMMA))advance();else break; }
                expect(T_RPAREN); call->left=head; return call;
            }
            Node*base=new_node(N_VAR); strcpy(base->name,t.text);
            Node*f=new_node(N_FIELD); f->left=base; strcpy(f->name,member.text); return f;
        }
        Node*n=new_node(N_VAR); strcpy(n->name,t.text); return n;
    }

    if (t.type == T_LPAREN) { advance(); Node*n=parse_expr(); expect(T_RPAREN); return n; }
    compile_error("unexpected token in expression"); return NULL;
}

Node *parse_unary(void) {
    if(check(T_MINUS)){advance();Node*n=new_node(N_NEG);n->left=parse_unary();return n;}
    return parse_primary();
}
Node *parse_multiplicative(void) {
    Node*l=parse_unary();
    while(check(T_STAR)||check(T_SLASH)||check(T_PERCENT)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_unary();l=n;}
    return l;
}
Node *parse_additive(void) {
    Node*l=parse_multiplicative();
    while(check(T_PLUS)||check(T_MINUS)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_multiplicative();l=n;}
    return l;
}
Node *parse_comparison(void) {
    Node*l=parse_additive();
    while(check(T_LT)||check(T_GT)||check(T_LE)||check(T_GE)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_additive();l=n;}
    return l;
}
Node *parse_equality(void) {
    Node*l=parse_comparison();
    while(check(T_EQEQ)||check(T_NE)){Token op=advance();Node*n=new_node(N_BINOP);n->val=op.type;n->left=l;n->right=parse_comparison();l=n;}
    return l;
}
Node *parse_logical_and(void) {
    Node*l=parse_equality();
    while(check(T_ANDAND)){advance();Node*n=new_node(N_BINOP);n->val=T_ANDAND;n->left=l;n->right=parse_equality();l=n;}
    return l;
}
Node *parse_logical_or(void) {
    Node*l=parse_logical_and();
    while(check(T_PIPEPIPE)){advance();Node*n=new_node(N_BINOP);n->val=T_PIPEPIPE;n->left=l;n->right=parse_logical_and();l=n;}
    return l;
}
Node *parse_expr(void) { return parse_logical_or(); }

Node *parse_block(void) {
    expect(T_LBRACE); Node*head=NULL,*tail=NULL;
    while(!check(T_RBRACE)&&!check(T_EOF)){Node*st=parse_stmt();if(!head)head=st;else tail->next=st;tail=st;}
    expect(T_RBRACE); return head;
}

Node *parse_stmt(void) {
    if(check(T_LET)){
        advance();Node*n=new_node(N_LET);int mut=0;
        if(check(T_MUT)){advance();mut=1;}
        Token name=expect_ident();strcpy(n->name,name.text);n->val=mut;
        if(check(T_COLON)){advance();parse_type_into(n->type);}
        expect(T_ASSIGN);n->left=parse_expr();expect(T_SEMI);return n;
    }
    if(check(T_IF)){
        advance();Node*n=new_node(N_IF);n->cond=parse_expr();n->body=parse_block();
        if(check(T_ELSE)){advance();if(check(T_IF))n->else_body=parse_stmt();else n->else_body=parse_block();}
        return n;
    }
    if(check(T_WHILE)){advance();Node*n=new_node(N_WHILE);n->cond=parse_expr();n->body=parse_block();return n;}
    if(check(T_RETURN)){advance();Node*n=new_node(N_RETURN);if(!check(T_SEMI))n->left=parse_expr();expect(T_SEMI);return n;}
    if(check(T_PRINT)){advance();Node*n=new_node(N_PRINT);expect(T_LPAREN);n->left=parse_expr();expect(T_RPAREN);expect(T_SEMI);return n;}
    if(check(T_DELETE)){advance();Node*n=new_node(N_DELETE);n->left=parse_expr();expect(T_SEMI);return n;}
    Node*expr=parse_expr();
    if(check(T_ASSIGN)){advance();Node*n=new_node(N_ASSIGN);n->left=expr;n->right=parse_expr();expect(T_SEMI);return n;}
    expect(T_SEMI); return expr;
}

void parse_struct(void) {
    expect(T_STRUCT);
    if(struct_count>=MAX_STRUCTS)compile_error("too many structs");
    StructDef*s=&structs[struct_count++];memset(s,0,sizeof(StructDef));
    Token name=expect_ident();strcpy(s->name,name.text);
    expect(T_LBRACE);
    while(!check(T_RBRACE)){
        if(s->field_count>=MAX_FIELDS)compile_error("too many fields");
        Token fn=expect_ident();strcpy(s->field_names[s->field_count],fn.text);
        expect(T_COLON);parse_type_into(s->field_types[s->field_count]);
        s->field_offsets[s->field_count]=s->field_count*16;s->field_count++;
        if(check(T_COMMA))advance();else break;
    }
    expect(T_RBRACE);s->size=s->field_count*16;
}

Node *parse_function(void) {
    expect(T_FN);Node*f=new_node(N_FUNC);Token name=expect_ident();strcpy(f->name,name.text);
    expect(T_LPAREN);
    while(!check(T_RPAREN)){
        if(f->param_count>=3)compile_error("max 3 params");
        Token pn=expect_ident();strcpy(f->param_names[f->param_count],pn.text);
        expect(T_COLON);parse_type_into(f->param_types[f->param_count]);f->param_count++;
        if(check(T_COMMA))advance();else break;
    }
    expect(T_RPAREN);
    if(check(T_ARROW)){advance();parse_type_into(f->ret_type);}else strcpy(f->ret_type,"void");
    f->body=parse_block();return f;
}

void parse_impl(Node **head, Node **tail) {
    expect(T_IMPL); Token st=expect_ident(); char sn[64]; strcpy(sn,st.text);
    expect(T_LBRACE);
    while(!check(T_RBRACE)){
        int is_static=0; if(check(T_STATIC)){advance();is_static=1;}
        expect(T_FN); Token mn=expect_ident(); expect(T_LPAREN);
        Node*f=new_node(N_FUNC); snprintf(f->name,sizeof(f->name),"%s_%s",sn,mn.text);
        int has_self=0,self_mut=0,tp=0;
        if(!is_static){
            if(check(T_MUT)){advance();expect(T_SELF);has_self=1;self_mut=1;}
            else if(check(T_SELF)){advance();has_self=1;}
            else compile_error("instance method requires self");
            strcpy(f->param_names[0],"self");strcpy(f->param_types[0],sn);tp=1;
            if(check(T_COMMA))advance();
        }
        while(!check(T_RPAREN)){
            if(tp>=3)compile_error("max params with self=3");
            Token pn=expect_ident();strcpy(f->param_names[tp],pn.text);
            expect(T_COLON);parse_type_into(f->param_types[tp]);tp++;
            if(check(T_COMMA))advance();else break;
        }
        expect(T_RPAREN);f->param_count=tp;
        if(check(T_ARROW)){advance();parse_type_into(f->ret_type);}else strcpy(f->ret_type,"void");
        f->val=self_mut; f->body=parse_block();
        if(method_count>=MAX_METHODS)compile_error("too many methods");
        MethodDef*m=&methods[method_count++];memset(m,0,sizeof(MethodDef));
        strcpy(m->struct_name,sn);strcpy(m->method_name,mn.text);strcpy(m->mangled,f->name);
        m->is_static=is_static;m->param_count=is_static?tp:tp-1;
        int as=is_static?0:1;for(int i=0;i<m->param_count;i++)strcpy(m->param_types[i],f->param_types[as+i]);
        strcpy(m->ret_type,f->ret_type);
        if(!*head)*head=f;else(*tail)->next=f;*tail=f;
    }
    expect(T_RBRACE);
}

Node *parse_program(void) {
    Node*head=NULL,*tail=NULL;
    while(!check(T_EOF)){
        if(check(T_STRUCT)){parse_struct();continue;}
        if(check(T_IMPORT)){advance();while(!check(T_SEMI)&&!check(T_EOF))advance();if(check(T_SEMI))advance();continue;}
        if(check(T_FN)){Node*f=parse_function();if(!head)head=f;else tail->next=f;tail=f;continue;}
        if(check(T_IMPL)){parse_impl(&head,&tail);continue;}
        compile_error("expected struct/fn/impl/import at top level");
    }
    return head;
}

// ------------------------------------------------------------
// Optimizer
// ------------------------------------------------------------
Node *fold_expr(Node *n);
Node *optimize_stmt(Node *n);
Node *make_num(int64_t v){Node*n=new_node(N_NUM);n->val=v;strcpy(n->type,"i64");return n;}

Node *optimize_list(Node *head) {
    Node*nh=NULL,**link=&nh;Node*s=head;
    while(s){Node*next=s->next;Node*os=optimize_stmt(s);if(os){os->next=NULL;*link=os;link=&os->next;}s=next;}
    return nh;
}

Node *fold_expr(Node *n) {
    if(!n)return NULL;
    switch(n->kind){
        case N_CALL:case N_METHOD_CALL:{
            Node**lk=&n->left;Node*a=n->left;
            while(a){Node*nx=a->next;Node*fa=fold_expr(a);fa->next=NULL;*lk=fa;lk=&fa->next;a=nx;}break;}
        case N_NEW:{Node**lk=&n->left;Node*a=n->left;while(a){Node*nx=a->next;Node*fa=fold_expr(a);fa->next=NULL;*lk=fa;lk=&fa->next;a=nx;}break;}
        case N_FIELD_INIT:n->left=fold_expr(n->left);break;
        case N_FIELD:if(n->left)n->left=fold_expr(n->left);break;
        case N_NEG:n->left=fold_expr(n->left);if(n->left->kind==N_NUM){n->left->val=-n->left->val;return n->left;}break;
        case N_BINOP:{
            n->left=fold_expr(n->left);n->right=fold_expr(n->right);
            if(n->left->kind==N_NUM&&n->right->kind==N_NUM){
                int64_t l=n->left->val,r=n->right->val;TokenType op=(TokenType)n->val;
                if(op==T_PLUS)return make_num(l+r);if(op==T_MINUS)return make_num(l-r);
                if(op==T_STAR)return make_num(l*r);if(op==T_SLASH&&r)return make_num(l/r);
                if(op==T_PERCENT&&r)return make_num(l%r);
                if(op==T_EQEQ)return make_num(l==r);if(op==T_NE)return make_num(l!=r);
                if(op==T_LT)return make_num(l<r);if(op==T_GT)return make_num(l>r);
                if(op==T_LE)return make_num(l<=r);if(op==T_GE)return make_num(l>=r);
                if(op==T_ANDAND)return make_num(l&&r);if(op==T_PIPEPIPE)return make_num(l||r);
            }break;}
        default:break;
    }
    return n;
}

Node *optimize_stmt(Node *n) {
    if(!n)return NULL;
    switch(n->kind){
        case N_LET:n->left=fold_expr(n->left);break;
        case N_ASSIGN:n->right=fold_expr(n->right);break;
        case N_IF:n->cond=fold_expr(n->cond);n->body=optimize_list(n->body);if(n->else_body)n->else_body=optimize_list(n->else_body);break;
        case N_WHILE:n->cond=fold_expr(n->cond);n->body=optimize_list(n->body);break;
        case N_PRINT:n->left=fold_expr(n->left);break;
        case N_RETURN:if(n->left)n->left=fold_expr(n->left);break;
        case N_DELETE:n->left=fold_expr(n->left);break;
        default:return fold_expr(n);
    }
    return n;
}

// ------------------------------------------------------------
// Emitter
// ------------------------------------------------------------
void emit_byte(uint8_t b){if(code_len>=CODE_SIZE)compile_error("code overflow");code_buf[code_len++]=b;}
void emit_bytes(const uint8_t*p,int n){for(int i=0;i<n;i++)emit_byte(p[i]);}
void emit_int32(int32_t v){if(code_len+4>CODE_SIZE)compile_error("code overflow");memcpy(&code_buf[code_len],&v,4);code_len+=4;}
void emit_int64(uint64_t v){if(code_len+8>CODE_SIZE)compile_error("code overflow");memcpy(&code_buf[code_len],&v,8);code_len+=8;}
void patch_int32(int p,int32_t v){memcpy(&code_buf[p],&v,4);}
void patch_int64(int p,uint64_t v){memcpy(&code_buf[p],&v,8);}
void emit_data_int64(uint64_t v){if(data_len+8>DATA_SIZE)compile_error("data overflow");memcpy(&data_buf[data_len],&v,8);data_len+=8;}
void emit_data_bytes(const uint8_t*p,int n){if(data_len+n>DATA_SIZE)compile_error("data overflow");memcpy(&data_buf[data_len],p,n);data_len+=n;}

void emit_mov_reg_reg(int d,int s){emit_byte(0x48);emit_byte(0x89);emit_byte(0xC0|(s<<3)|d);}
void emit_mov_rbp_disp_reg(int disp,int reg){emit_byte(0x48);emit_byte(0x89);emit_byte(0x85|(reg<<3));emit_int32(disp);}
void emit_mov_reg_rbp_disp(int reg,int disp){emit_byte(0x48);emit_byte(0x8B);emit_byte(0x85|(reg<<3));emit_int32(disp);}
void emit_mov_mem_base_disp_reg(int base,int disp,int reg){emit_byte(0x48);emit_byte(0x89);emit_byte(0x80|(reg<<3)|base);emit_int32(disp);}
void emit_mov_reg_mem_base_disp(int reg,int base,int disp){emit_byte(0x48);emit_byte(0x8B);emit_byte(0x80|(reg<<3)|base);emit_int32(disp);}
void emit_mov_imm64(int reg,uint64_t v){emit_byte(0x48);emit_byte(0xB8+reg);emit_int64(v);}
int emit_mov_imm64_patch(int reg){emit_byte(0x48);emit_byte(0xB8+reg);int p=code_len;emit_int64(0);return p;}
void emit_push_reg(int reg){emit_byte(0x50+reg);}
void emit_pop_reg(int reg){emit_byte(0x58+reg);}
void emit_test_rax(void){emit_byte(0x48);emit_byte(0x85);emit_byte(0xC0);}
void emit_boolize(void){emit_test_rax();emit_byte(0x0F);emit_byte(0x95);emit_byte(0xC0);emit_byte(0x48);emit_byte(0x0F);emit_byte(0xB6);emit_byte(0xC0);}
void emit_setcc(uint8_t op){emit_byte(0x0F);emit_byte(op);emit_byte(0xC0);emit_byte(0x48);emit_byte(0x0F);emit_byte(0xB6);emit_byte(0xC0);}

int new_label(void){if(label_count>=MAX_PATCHES)compile_error("label overflow");label_pos[label_count]=-1;return label_count++;}
void add_patch(int label){if(patch_count>=MAX_PATCHES)compile_error("patch overflow");patches[patch_count].pos=code_len;patches[patch_count].target=label;patch_count++;emit_int32(0);}
void emit_je_label(int l){emit_byte(0x0F);emit_byte(0x84);if(label_pos[l]!=-1){emit_int32(label_pos[l]-(code_len+4));}else add_patch(l);}
void emit_jnz_label(int l){emit_byte(0x0F);emit_byte(0x85);if(label_pos[l]!=-1){emit_int32(label_pos[l]-(code_len+4));}else add_patch(l);}
void emit_jmp_label(int l){emit_byte(0xE9);if(label_pos[l]!=-1){emit_int32(label_pos[l]-(code_len+4));}else add_patch(l);}
void set_label(int l){label_pos[l]=code_len;for(int i=0;i<patch_count;i++){if(patches[i].target==l){patch_int32(patches[i].pos,code_len-(patches[i].pos+4));patches[i].target=-1;}}}

void emit_call_known(int t){emit_byte(0xE8);emit_int32(t-(code_len+4));}
void emit_call_symbol(const char*name){emit_byte(0xE8);if(call_site_count>=MAX_CALL_SITES)compile_error("callsite overflow");call_sites[call_site_count].pos=code_len;strcpy(call_sites[call_site_count].name,name);call_site_count++;emit_int32(0);}

int find_func(const char*name){for(int i=0;i<sym_count;i++)if(sym_table[i].is_func&&strcmp(sym_table[i].name,name)==0)return i;return -1;}
int find_var(const char*name){for(int i=sym_count-1;i>=0;i--)if(!sym_table[i].is_func&&strcmp(sym_table[i].name,name)==0)return i;return -1;}
int add_function_symbol(Node*f){if(sym_count>=MAX_SYMS)compile_error("sym overflow");Symbol*s=&sym_table[sym_count];memset(s,0,sizeof(Symbol));strcpy(s->name,f->name);s->is_func=1;s->code_offset=-1;s->param_count=f->param_count;for(int i=0;i<f->param_count;i++)strcpy(s->param_types[i],f->param_types[i]);strcpy(s->ret_type,f->ret_type);return sym_count++;}
int add_local(const char*name,const char*type,int offset,int is_mut){if(sym_count>=MAX_SYMS)compile_error("sym overflow");Symbol*s=&sym_table[sym_count];memset(s,0,sizeof(Symbol));strcpy(s->name,name);strcpy(s->type,type);s->offset=offset;s->is_mut=is_mut;return sym_count++;}

int add_string_lit(const char*text){
    for(int i=0;i<string_pool_count;i++)if(strcmp(string_pool[i].text,text)==0)return i;
    if(string_pool_count>=MAX_STRINGS)compile_error("strpool overflow");
    int len=(int)strlen(text);emit_data_int64(1);int so=data_len;
    emit_data_bytes((const uint8_t*)text,len);uint8_t z=0;emit_data_bytes(&z,1);
    strcpy(string_pool[string_pool_count].text,text);string_pool[string_pool_count].len=len;string_pool[string_pool_count].data_offset=so;
    return string_pool_count++;
}

void record_heap_patch(int kind){if(heap_patch_count>=MAX_HEAP_PATCHES)compile_error("heap patch overflow");heap_patches[heap_patch_count].pos=code_len;heap_patches[heap_patch_count].kind=kind;heap_patch_count++;emit_int64(0);}

void emit_alloc_runtime(void){
    f_alloc_off=code_len;
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
    emit_byte(0x48);emit_byte(0x89);emit_byte(0xC8);emit_byte(0xC3);
}

static const uint8_t print_int_code[]={0x55,0x48,0x89,0xE5,0x48,0x83,0xEC,0x40,0x48,0x89,0xF8,0x48,0x8D,0x75,0xFF,0xC6,0x06,0x00,0x48,0xC7,0xC1,0x00,0x00,0x00,0x00,0x45,0x31,0xC0,0x48,0x83,0xF8,0x00,0x79,0x0A,0x48,0xF7,0xD8,0x49,0xC7,0xC0,0x01,0x00,0x00,0x00,0x48,0x31,0xD2,0x49,0xC7,0xC2,0x0A,0x00,0x00,0x00,0x49,0xF7,0xF2,0x80,0xC2,0x30,0x48,0xFF,0xCE,0x88,0x16,0x48,0xFF,0xC1,0x48,0x85,0xC0,0x75,0xE3,0x4D,0x85,0xC0,0x74,0x09,0x48,0xFF,0xCE,0xC6,0x06,0x2D,0x48,0xFF,0xC1,0x48,0x89,0xCA,0x48,0xC7,0xC7,0x01,0x00,0x00,0x00,0x48,0xC7,0xC0,0x01,0x00,0x00,0x00,0x0F,0x05,0xC9,0xC3};
static const uint8_t print_str_code[]={0x48,0x89,0xF2,0x48,0x89,0xFE,0x48,0xC7,0xC7,0x01,0x00,0x00,0x00,0x48,0xC7,0xC0,0x01,0x00,0x00,0x00,0x0F,0x05,0xC3};
static const uint8_t f_retain_code[]={0x48,0x85,0xFF,0x74,0x0B,0x48,0x8B,0x47,0xF8,0x48,0xFF,0xC0,0x48,0x89,0x47,0xF8,0xC3};
static const uint8_t f_release_code[]={0x48,0x85,0xFF,0x74,0x0B,0x48,0x8B,0x47,0xF8,0x48,0xFF,0xC8,0x48,0x89,0x47,0xF8,0xC3};

void emit_runtime(void){
    emit_alloc_runtime();
    print_int_off=code_len;emit_bytes(print_int_code,sizeof(print_int_code));
    print_str_off=code_len;emit_bytes(print_str_code,sizeof(print_str_code));
    f_retain_off=code_len;emit_bytes(f_retain_code,sizeof(f_retain_code));
    f_release_off=code_len;emit_bytes(f_release_code,sizeof(f_release_code));
    start_off=code_len;emit_call_symbol("main");
    emit_mov_reg_reg(7,0);emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC0);emit_int32(60);emit_byte(0x0F);emit_byte(0x05);
}

// ------------------------------------------------------------
// Codegen
// ------------------------------------------------------------
void compile_expr(Node *n);
void compile_stmt(Node *n);
void compile_block(Node *block);

void emit_retain_rax_rdx(void){emit_push_reg(0);emit_push_reg(2);emit_mov_reg_reg(7,0);emit_call_known(f_retain_off);emit_pop_reg(2);emit_pop_reg(0);}

void compile_expr(Node *n) {
    if(!n)return;
    switch(n->kind){
    case N_NUM: if(n->type[0]=='\0')strcpy(n->type,"i64"); emit_mov_imm64(0,(uint64_t)n->val); break;
    case N_STR: {
        int id=add_string_lit(n->name);int p=emit_mov_imm64_patch(0);
        if(string_patch_count>=MAX_STRING_PATCHES)compile_error("strpatch overflow");
        string_patches[string_patch_count].pos=p;string_patches[string_patch_count].str_id=id;string_patch_count++;
        emit_mov_imm64(2,(uint64_t)string_pool[id].len);strcpy(n->type,"str");break;}
    case N_VAR: {
        int idx=find_var(n->name);if(idx<0)compile_error("undef var");strcpy(n->type,sym_table[idx].type);
        emit_mov_reg_rbp_disp(0,sym_table[idx].offset);
        if(strcmp(n->type,"str")==0)emit_mov_reg_rbp_disp(2,sym_table[idx].offset+8);break;}
    case N_NEG: compile_expr(n->left);if(!is_int_type(n->left->type))compile_error("neg requires int");emit_byte(0x48);emit_byte(0xF7);emit_byte(0xD8);strcpy(n->type,n->left->type);break;
    case N_FIELD: {
        if(!n->left||n->left->kind!=N_VAR)compile_error("only var.field");
        int vi=find_var(n->left->name);if(vi<0)compile_error("undef struct var");
        StructDef*sd=find_struct(sym_table[vi].type);if(!sd)compile_error("not struct");
        int fi=find_field(sd,n->name);if(fi<0)compile_error("undef field");
        int off=sd->field_offsets[fi];emit_mov_reg_rbp_disp(0,sym_table[vi].offset);
        if(strcmp(sd->field_types[fi],"str")==0){emit_mov_reg_mem_base_disp(0,0,off);emit_mov_reg_mem_base_disp(2,0,off+8);}
        else emit_mov_reg_mem_base_disp(0,0,off);
        strcpy(n->type,sd->field_types[fi]);break;}
    case N_NEW: {
        StructDef*sd=find_struct(n->name);if(!sd)compile_error("undef struct");
        emit_mov_imm64(7,(uint64_t)(sd->size+8));emit_call_known(f_alloc_off);
        emit_byte(0x48);emit_byte(0xC7);emit_byte(0xC1);emit_int32(1);
        emit_byte(0x48);emit_byte(0x89);emit_byte(0x08);
        emit_byte(0x48);emit_byte(0x83);emit_byte(0xC0);emit_byte(0x08);
        emit_push_reg(3);emit_mov_reg_reg(3,0);
        int mask=0;
        for(Node*init=n->left;init;init=init->next){
            if(init->kind!=N_FIELD_INIT)compile_error("bad init");
            int fi=find_field(sd,init->name);if(fi<0)compile_error("unknown field");
            if(mask&(1<<fi))compile_error("dup field");mask|=(1<<fi);
            compile_expr(init->left);if(!types_compatible(sd->field_types[fi],init->left->type))compile_error("field type mismatch");
            int off=sd->field_offsets[fi];
            if(strcmp(sd->field_types[fi],"str")==0){emit_retain_rax_rdx();emit_mov_mem_base_disp_reg(3,off,0);emit_mov_mem_base_disp_reg(3,off+8,2);}
            else emit_mov_mem_base_disp_reg(3,off,0);
        }
        if(mask!=(1<<sd->field_count)-1)compile_error("missing fields");
        emit_mov_reg_reg(0,3);emit_pop_reg(3);strcpy(n->type,sd->name);break;}
    case N_METHOD_CALL: {
        StructDef*sd=find_struct(n->aux);int is_static=(sd!=NULL);int obj_var=-1;
        if(!is_static){obj_var=find_var(n->aux);if(obj_var<0)compile_error("undef method base");sd=find_struct(sym_table[obj_var].type);if(!sd)compile_error("base not struct");}
        MethodDef*m=find_method(sd->name,n->name,is_static);if(!m)compile_error("undef method");
        Node*args[3];int argc=0;for(Node*a=n->left;a;a=a->next){if(argc>=3)compile_error("too many args");args[argc++]=a;}
        if(argc!=m->param_count)compile_error("method arg count mismatch");
        if(is_static&&argc>3)compile_error("too many static args");
        if(!is_static&&argc>2)compile_error("too many instance args");
        for(int i=0;i<argc;i++){compile_expr(args[i]);if(!types_compatible(m->param_types[i],args[i]->type))compile_error("method arg type mismatch");emit_push_reg(0);}
        if(!is_static){emit_mov_reg_rbp_disp(0,sym_table[obj_var].offset);emit_mov_reg_reg(7,0);}
        for(int i=argc-1;i>=0;i--){int reg;if(is_static)reg=(i==0)?7:(i==1?6:2);else reg=(i==0)?6:(i==1?2:2);emit_pop_reg(reg);}
        emit_call_symbol(m->mangled);strcpy(n->type,m->ret_type);break;}
    case N_BINOP: {
        TokenType op=(TokenType)n->val;
        if(op==T_ANDAND||op==T_PIPEPIPE){
            compile_expr(n->left);if(!is_int_type(n->left->type))compile_error("logical needs scalar");emit_boolize();emit_push_reg(0);
            compile_expr(n->right);if(!is_int_type(n->right->type))compile_error("logical needs scalar");emit_boolize();emit_pop_reg(3);
            if(op==T_ANDAND){emit_byte(0x48);emit_byte(0x21);emit_byte(0xD8);}else{emit_byte(0x48);emit_byte(0x09);emit_byte(0xD8);}
            strcpy(n->type,"i64");break;}
        compile_expr(n->left);emit_push_reg(0);compile_expr(n->right);emit_pop_reg(3);
        char lt[16],rt[16];strcpy(lt,n->left->type);strcpy(rt,n->right->type);
        if(!is_int_type(lt)||!is_int_type(rt))compile_error("op requires int");
        switch(op){
            case T_PLUS:emit_byte(0x48);emit_byte(0x01);emit_byte(0xD8);break;
            case T_MINUS:emit_byte(0x48);emit_byte(0x29);emit_byte(0xC3);emit_mov_reg_reg(0,3);break;
            case T_STAR:emit_byte(0x48);emit_byte(0x0F);emit_byte(0xAF);emit_byte(0xC3);break;
            case T_SLASH:case T_PERCENT:emit_mov_reg_reg(1,0);emit_mov_reg_reg(0,3);emit_byte(0x48);emit_byte(0x31);emit_byte(0xD2);emit_byte(0x48);emit_byte(0xF7);emit_byte(0xF9);if(op==T_PERCENT)emit_mov_reg_reg(0,2);break;
            case T_EQEQ:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x94);break;
            case T_NE:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x95);break;
            case T_LT:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9C);break;
            case T_GE:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9D);break;
            case T_LE:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9E);break;
            case T_GT:emit_byte(0x48);emit_byte(0x39);emit_byte(0xC3);emit_setcc(0x9F);break;
            default:compile_error("unsupported binop");}
        if(op==T_EQEQ||op==T_NE||op==T_LT||op==T_GT||op==T_LE||op==T_GE)strcpy(n->type,"i64");
        else{if(strcmp(lt,rt)==0)strcpy(n->type,lt);else strcpy(n->type,"i64");}break;}
    case N_CALL: {
        int fi=find_func(n->name);if(fi<0)compile_error("undef func");Symbol*f=&sym_table[fi];
        Node*args[3];int argc=0;for(Node*a=n->left;a;a=a->next){if(argc>=3)compile_error("too many args");args[argc++]=a;}
        if(argc!=f->param_count)compile_error("arg count mismatch");
        for(int i=0;i<argc;i++)if(!is_int_type(f->param_types[i]))compile_error("only int params in core");
        if(argc>=1){compile_expr(args[0]);if(!types_compatible(f->param_types[0],args[0]->type))compile_error("arg type mismatch");if(argc==1)emit_mov_reg_reg(7,0);else emit_push_reg(0);}
        if(argc>=2){compile_expr(args[1]);if(!types_compatible(f->param_types[1],args[1]->type))compile_error("arg type mismatch");if(argc==2){emit_mov_reg_reg(6,0);emit_pop_reg(7);}else emit_push_reg(0);}
        if(argc>=3){compile_expr(args[2]);if(!types_compatible(f->param_types[2],args[2]->type))compile_error("arg type mismatch");emit_mov_reg_reg(2,0);emit_pop_reg(6);emit_pop_reg(7);}
        emit_call_symbol(n->name);strcpy(n->type,f->ret_type);break;}
    default:compile_error("invalid expr");}
}

void compile_block(Node*b){for(Node*s=b;s;s=s->next)compile_stmt(s);}

void compile_let(Node*n){
    compile_expr(n->left);if(strcmp(n->left->type,"void")==0)compile_error("cannot bind void");
    char vt[16];if(n->type[0]){if(!types_compatible(n->type,n->left->type))compile_error("decl type mismatch");strcpy(vt,n->type);}else strcpy(vt,n->left->type);
    if(find_var(n->name)!=-1)compile_error("var exists");int mut=(int)n->val;
    StructDef*sd=find_struct(vt);
    if(sd){if(n->left->kind!=N_NEW)emit_retain_rax_rdx();stack_offset-=16;add_local(n->name,vt,stack_offset,mut);emit_mov_rbp_disp_reg(stack_offset,0);}
    else if(strcmp(vt,"str")==0){emit_retain_rax_rdx();stack_offset-=16;int off=stack_offset;add_local(n->name,vt,off,mut);emit_mov_rbp_disp_reg(off,0);emit_mov_rbp_disp_reg(off+8,2);}
    else{stack_offset-=16;int off=stack_offset;add_local(n->name,vt,off,mut);emit_mov_rbp_disp_reg(off,0);}
}

void compile_assign_field(Node*n){
    if(!n->left||n->left->kind!=N_FIELD)compile_error("bad field assign");
    Node*field=n->left;if(!field->left||field->left->kind!=N_VAR)compile_error("only var.field assign");
    int vi=find_var(field->left->name);if(vi<0)compile_error("undef struct var");
    Symbol*sym=&sym_table[vi];if(!sym->is_mut)compile_error("immutable struct");
    StructDef*sd=find_struct(sym->type);if(!sd)compile_error("not struct");
    int fi=find_field(sd,field->name);if(fi<0)compile_error("undef field");
    int off=sd->field_offsets[fi];const char*ft=sd->field_types[fi];
    if(strcmp(ft,"str")==0){
        compile_expr(n->right);if(!types_compatible(ft,n->right->type))compile_error("field type mismatch");
        emit_push_reg(0);emit_push_reg(2);emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(3,0);
        emit_mov_reg_mem_base_disp(7,3,off);emit_call_known(f_release_off);emit_pop_reg(2);emit_pop_reg(0);
        emit_retain_rax_rdx();emit_mov_mem_base_disp_reg(3,off,0);emit_mov_mem_base_disp_reg(3,off+8,2);
    }else{
        compile_expr(n->right);if(!types_compatible(ft,n->right->type))compile_error("field type mismatch");
        emit_push_reg(0);emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(3,0);emit_pop_reg(0);
        emit_mov_mem_base_disp_reg(3,off,0);
    }
}

void compile_assign(Node*n){
    if(n->left->kind==N_FIELD){compile_assign_field(n);return;}
    if(n->left->kind!=N_VAR)compile_error("assign to var or field");
    int idx=find_var(n->left->name);if(idx<0)compile_error("undef var");
    Symbol*sym=&sym_table[idx];if(!sym->is_mut)compile_error("immutable var");
    StructDef*sd=find_struct(sym->type);
    if(sd){emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(7,0);emit_call_known(f_release_off);compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("assign type mismatch");if(n->right->kind!=N_NEW)emit_retain_rax_rdx();emit_mov_rbp_disp_reg(sym->offset,0);}
    else if(strcmp(sym->type,"str")==0){emit_mov_reg_rbp_disp(0,sym->offset);emit_mov_reg_reg(7,0);emit_call_known(f_release_off);compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("assign type mismatch");emit_retain_rax_rdx();emit_mov_rbp_disp_reg(sym->offset,0);emit_mov_rbp_disp_reg(sym->offset+8,2);}
    else{compile_expr(n->right);if(!types_compatible(sym->type,n->right->type))compile_error("assign type mismatch");emit_mov_rbp_disp_reg(sym->offset,0);}
}

void compile_print(Node*n){compile_expr(n->left);if(strcmp(n->left->type,"str")==0){emit_mov_reg_reg(7,0);emit_mov_reg_reg(6,2);emit_call_known(print_str_off);}else if(is_int_type(n->left->type)){emit_mov_reg_reg(7,0);emit_call_known(print_int_off);}else compile_error("print supports int/str only");}
void compile_if(Node*n){int el=new_label(),en=new_label();compile_expr(n->cond);if(strcmp(n->cond->type,"str")==0||strcmp(n->cond->type,"void")==0)compile_error("cond must be scalar");emit_test_rax();emit_je_label(el);compile_block(n->body);if(n->else_body){emit_jmp_label(en);set_label(el);compile_block(n->else_body);set_label(en);}else set_label(el);}
void compile_while(Node*n){int sl=new_label(),el=new_label();set_label(sl);compile_expr(n->cond);if(strcmp(n->cond->type,"str")==0||strcmp(n->cond->type,"void")==0)compile_error("cond must be scalar");emit_test_rax();emit_je_label(el);compile_block(n->body);emit_jmp_label(sl);set_label(el);}
void compile_return(Node*n){if(current_func_sym<0)compile_error("return outside fn");Symbol*f=&sym_table[current_func_sym];if(strcmp(f->ret_type,"void")==0){if(n->left)compile_error("void fn cannot return value");}else{if(!n->left)compile_error("return missing value");compile_expr(n->left);if(!types_compatible(f->ret_type,n->left->type))compile_error("return type mismatch");}emit_byte(0xC9);emit_byte(0xC3);}
void compile_delete(Node*n){compile_expr(n->left);if(strcmp(n->left->type,"str")==0||find_struct(n->left->type)){emit_mov_reg_reg(7,0);emit_call_known(f_release_off);}else compile_error("delete supports str/struct only");}

void compile_stmt(Node*n){if(!n)return;switch(n->kind){case N_LET:compile_let(n);break;case N_ASSIGN:compile_assign(n);break;case N_IF:compile_if(n);break;case N_WHILE:compile_while(n);break;case N_PRINT:compile_print(n);break;case N_RETURN:compile_return(n);break;case N_DELETE:compile_delete(n);break;default:compile_expr(n);break;}}

void compile_function(Node*f){
    int idx=find_func(f->name);if(idx<0)compile_error("func sym missing");
    sym_table[idx].code_offset=code_len;current_func_sym=idx;sym_count=global_sym_count;stack_offset=0;
    emit_byte(0x55);emit_byte(0x48);emit_byte(0x89);emit_byte(0xE5);emit_byte(0x48);emit_byte(0x81);emit_byte(0xEC);emit_int32(256);
    for(int i=0;i<f->param_count;i++){
        stack_offset-=16;int pm=0;if(i==0&&strcmp(f->param_names[i],"self")==0)pm=(int)f->val;
        add_local(f->param_names[i],f->param_types[i],stack_offset,pm);
        int reg=(i==0)?7:(i==1?6:2);emit_mov_rbp_disp_reg(stack_offset,reg);
    }
    compile_block(f->body);emit_byte(0x48);emit_byte(0x31);emit_byte(0xC0);emit_byte(0xC9);emit_byte(0xC3);
}

void patch_calls(void){for(int i=0;i<call_site_count;i++){int fi=find_func(call_sites[i].name);if(fi<0){fprintf(stderr,"Link Error: undef '%s'\n",call_sites[i].name);exit(1);}int t=sym_table[fi].code_offset;if(t<0){fprintf(stderr,"Link Error: no body '%s'\n",call_sites[i].name);exit(1);}patch_int32(call_sites[i].pos,t-(call_sites[i].pos+4));}}
void patch_strings(void){int fl=code_len;for(int i=0;i<string_patch_count;i++){int sid=string_patches[i].str_id;uint64_t addr=RUNTIME_BASE+(uint64_t)fl+(uint64_t)string_pool[sid].data_offset;patch_int64(string_patches[i].pos,addr);}}
void patch_heap(uint64_t tf){uint64_t hp=LOAD_ADDR+tf,hs=hp+16;for(int i=0;i<heap_patch_count;i++)patch_int64(heap_patches[i].pos,heap_patches[i].kind==0?hp:hs);}

void write_output(const char*filename){
    FILE*fp=fopen(filename,"wb");if(!fp)compile_error("cannot open output");
    uint64_t tf=FILE_HEADER_SIZE+code_len+data_len,entry=RUNTIME_BASE+start_off,ms=tf+HEAP_SIZE;
    uint8_t ident[16]={0};ident[0]=0x7F;ident[1]='E';ident[2]='L';ident[3]='F';ident[4]=2;ident[5]=1;ident[6]=1;fwrite(ident,1,16,fp);
    uint16_t v16;uint32_t v32;uint64_t v64;
    v16=2;fwrite(&v16,2,1,fp);v16=62;fwrite(&v16,2,1,fp);v32=1;fwrite(&v32,4,1,fp);
    fwrite(&entry,8,1,fp);v64=64;fwrite(&v64,8,1,fp);v64=0;fwrite(&v64,8,1,fp);v32=0;fwrite(&v32,4,1,fp);
    v16=64;fwrite(&v16,2,1,fp);v16=56;fwrite(&v16,2,1,fp);v16=1;fwrite(&v16,2,1,fp);v16=0;fwrite(&v16,2,1,fp);v16=0;fwrite(&v16,2,1,fp);v16=0;fwrite(&v16,2,1,fp);
    v32=1;fwrite(&v32,4,1,fp);v32=7;fwrite(&v32,4,1,fp);v64=0;fwrite(&v64,8,1,fp);v64=LOAD_ADDR;fwrite(&v64,8,1,fp);v64=LOAD_ADDR;fwrite(&v64,8,1,fp);
    fwrite(&tf,8,1,fp);fwrite(&ms,8,1,fp);v64=0x1000;fwrite(&v64,8,1,fp);
    fwrite(code_buf,1,code_len,fp);fwrite(data_buf,1,data_len,fp);fclose(fp);
    printf("✅ %s generated (%llu bytes)\n",filename,(unsigned long long)tf);
}

int main(int argc,char**argv){
    if(argc<3){printf("Usage: %s <input.fs> <output.ft>\n",argv[0]);return 1;}
    process_file(argv[1]);source_buf[source_len]='\0';
    tokenize(source_buf);Node*funcs=parse_program();if(!funcs)compile_error("no functions");
    for(Node*f=funcs;f;f=f->next)f->body=optimize_list(f->body);
    for(Node*f=funcs;f;f=f->next){if(find_func(f->name)!=-1)compile_error("dup func");add_function_symbol(f);}
    if(find_func("main")==-1)compile_error("main not found");
    global_sym_count=sym_count;emit_runtime();
    for(Node*f=funcs;f;f=f->next)compile_function(f);
    patch_calls();patch_strings();uint64_t tf=FILE_HEADER_SIZE+code_len+data_len;patch_heap(tf);write_output(argv[2]);return 0;
}
