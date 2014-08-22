/* (CORB-XXX) Are the rules taken from "Common Object Request Broker
              Architecture (CORBA) Specification, Version 3.2 - Part 1: CORBA
              Interfaces." 
   
   Afterwards, the changes listed in "Extensible and Dynamic Topic Types for
   DDS 1.0" were applied. As they are not numbered in this document, we include
   a full copy of the modifications here, with our own numbering scheme applied:
 
   7.3.1.12.1 New Productions

   The following new productions are defined:
   
   (XTYP-101) <annotation> ::= <ann_dcl>
                             | <ann_fwd_dcl>
   (XTYP-102) <ann_dcl> ::= <ann_header> "{" <ann_body> "}"
   (XTYP-103) <ann_fwd_dcl> ::= "@annotation [ "(" ")" ] local interface" <identifier>
   (XTYP-104) <ann_header> ::= "@annotation [ "(" ")" ] local interface" <identifier> [ <ann_inheritance_spec> ]
   (XTYP-105) <ann_body> ::= <ann_attr>*
   (XTYP-106) <ann_inheritance_spec> ::= ":" <annotation_name>
   (XTYP-107) <annotation_name> ::= <scoped_name>
   (XTYP-108) <ann_attr> ::= <ann_appl> "attribute" <param_type_spec> <simple_declarator> [ "default" <const_exp> ] ";" <ann_appl_post>
   (XTYP-109) <ann_appl> ::= { "@" <ann_appl_dcl> }*
   (XTYP-110) <ann_appl_post> ::= { "//@" <ann_appl_dcl> }*
   (XTYP-111) <ann_appl_dcl> ::= <annotation_name> [ "(" [ <ann_appl_params> ] ")" ]
   (XTYP-112) <ann_appl_params> ::= <const_exp>
                                  | <ann_appl_param> { "," <ann_appl_param> }*
   (XTYP-113) <ann_appl_param> ::= <identifier> "=" <const_exp>
   (XTYP-114) <struct_header> ::= <ann_appl> "struct" <identifier> [ ":" <scoped_name> ]
   (XTYP-115) <switch_type_name> ::= <integer_type>
                                   | <char_type>
                                   | <wide_char_type>
                                   | <boolean_type>
                                   | <enum_type>
                                   | <octet_type>
                                   | <scoped_name>
   (XTYP-116) <map_type> ::= "map" "<" <simple_type_spec> "," <ann_appl> <simple_type_spec> "," <ann_appl_post> <positive_int_const> ">"
                           | "map" "<" <simple_type_spec> "," <ann_appl> <simple_type_spec> <ann_appl_post> ">"

   7.3.1.12.2 Modified Productions
   
   The following productions from [IDL] are extended:

   (XTYP-201) <union_type> ::= <ann_appl> ...
   (XTYP-202) <switch_type_spec> ::= <ann_appl> <switch_type_name> <ann_appl_post>
   (XTYP-203) <member> ::= ...
                         | <ann_appl> <type_spec> <declarator> ";" <ann_appl_post>
   (XTYP-204) <case> ::= ... <ann_appl_post>
   (XTYP-205) <element_spec> ::= <ann_appl> ...
   (XTYP-206) <enumerator> ::= <ann_appl> ...
   (XTYP-207) <template_type_spec> ::= ... | <map_type>

   The following productions from [IDL] are replaced:
   
   (XTYP-208) <struct_type> ::= <struct_header> "{" <member_list> "}"
   (XTYP-209) <switch_type_spec> ::= <ann_appl> <switch_type_name> <ann_appl_post>
   (XTYP-210) <enum_type> ::= <ann_appl> "enum" <identifier> "{" <enumerator> { "," <ann_appl_post> <enumerator> }* <ann_appl_post> "}"
   (XTYP-211) <sequence_type> ::= "sequence" "<" <ann_appl> <simple_type_spec> "," <ann_appl_post> <positive_int_const> ">"
                                | "sequence" "<" <ann_appl> <simple_type_spec> <ann_appl_post> ">"
   (XTYP-212) <array_declarator> ::= <identifier> <ann_appl> <ann_appl_post> <fixed_array_size>+
   
   The <definitions> production from [IDL] is modified as follows:
   
   (XTYP-213) <definition> ::= <type_dcl> ";" <ann_appl_post>
                             | ...
                             | <annotation> ";" <ann_appl_post> */
%{

extern int idllex (void);
extern void idlerror (char *s);
#include <stdlib.h>
#include <string.h>
#define YYDEBUG 1
#include "typeobj.h"
#include "typeobj_manip.h"
#include "string_manip.h"
#include "idlparser_support.h"

%}

%union {
    char            	*name;
    int                 val;
    Type            	*type;
    MemberList		*memberlist;
    Member		*member;
    EnumConstList	*enumconstlist;
    EnumConst 		*enumconst;
    DeclaratorList      *declist;
    TypeDeclaratorList  *typedeclist;
    ArraySize          *arraysize;
    Literal		literal;
    AnnotationUsage 	*ann_appl;
    UnionMemberList	*unionmemberlist;
    UnionMember		*unionmember;
    LabelList		*labellist;
}


%token T_IDENTIFIER
%token T_KW_ABSTRACT
%token T_KW_ANNOTATION
%token T_KW_ANY
%token T_KW_ATTRIBUTE
%token T_KW_BOOLEAN
%token T_KW_CASE
%token T_KW_CHAR
%token T_KW_COMPONENT
%token T_KW_CONST
%token T_KW_CONSUMES
%token T_KW_CONTEXT
%token T_KW_CUSTOM
%token T_KW_DEFAULT
%token T_KW_DOUBLE
%token T_KW_EMITS
%token T_KW_ENUM
%token T_KW_EVENTTYPE
%token T_KW_EXCEPTION
%token T_KW_FACTORY
%token T_KW_FALSE
%token T_KW_FINDER
%token T_KW_FIXED
%token T_KW_FLOAT
%token T_KW_GETRAISES
%token T_KW_HOME
%token T_KW_IMPORT
%token T_KW_IN
%token T_KW_INOUT
%token T_KW_INTERFACE
%token T_KW_LOCAL
%token T_KW_LONG
%token T_KW_MODULE
%token T_KW_MULTIPLE
%token T_KW_NATIVE
%token T_KW_OBJECT
%token T_KW_OCTET
%token T_KW_ONEWAY
%token T_KW_OUT
%token T_KW_PRIMARYKEY
%token T_KW_PRIVATE
%token T_KW_PROVIDES
%token T_KW_PUBLIC
%token T_KW_PUBLISHES
%token T_KW_RAISES
%token T_KW_READONLY
%token T_KW_SEQUENCE
%token T_KW_SETRAISES
%token T_KW_SHORT
%token T_KW_STRING
%token T_KW_STRUCT
%token T_KW_SUPPORTS
%token T_KW_SWITCH
%token T_KW_TRUE
%token T_KW_TRUNCATABLE
%token T_KW_TYPEDEF
%token T_KW_TYPEID
%token T_KW_TYPEPREFIX
%token T_KW_UNION
%token T_KW_UNSIGNED
%token T_KW_USES
%token T_KW_VALUEBASE
%token T_KW_VALUETYPE
%token T_KW_VOID
%token T_KW_WCHAR
%token T_KW_WSTRING
%token T_L_CHAR
%token T_L_FIXED
%token T_L_FLOAT
%token T_L_INTEGER
%token T_L_WCHAR
%token T_L_WSTRING
%token T_L_STRING
%token T_P_AND
%token T_P_AT
%token T_P_BACK
%token T_P_CIRC
%token T_P_COLON
%token T_P_COMMA
%token T_P_C_A
%token T_P_C_C
%token T_P_C_R
%token T_P_C_S
%token T_P_DOUBLE_COLON
%token T_P_IS
%token T_P_MIN
%token T_P_O_A
%token T_P_O_C
%token T_P_O_R
%token T_P_O_S
%token T_P_PERCENT
%token T_P_PIPE
%token T_P_PLUS
%token T_P_SEMI
%token T_P_SHIFT_L
%token T_P_SHIFT_R
%token T_P_SLASH
%token T_P_SLASHAT
%token T_P_STAR
%token T_P_TILDE
%token T_EOL
%token T_KW_MAP


%type <type> base_type_spec boolean_type char_type floating_pt_type integer_type octet_type signed_int signed_long_int signed_longlong_int signed_short_int simple_type_spec unsigned_int unsigned_long_int unsigned_longlong_int unsigned_short_int wide_char_type wide_string_type fixed_pt_type template_type_spec map_type string_type sequence_type  value_base_type object_type any_type struct_header struct_type struct_header_scoped_name_empty constr_type_spec union_type enum_type type_spec switch_type_spec switch_type_name
%type <name> scoped_name T_IDENTIFIER annotation_name
%type <val> positive_int_const fixed_array_size;
%type <enumconstlist> enumerators;
%type <memberlist> member_list member;
%type <enumconst> enumerator;
%type <arraysize> fixed_array_sizes;
%type <declist> declarators declarator multiple_member_declarators member_declarator simple_declarator complex_declarator array_declarator
%type <literal> literal boolean_literal T_L_CHAR T_L_FIXED T_L_FLOAT T_L_INTEGER T_L_WCHAR T_L_WSTRING T_L_STRING
%type <ann_appl> ann_appl_dcl ann_appl_post ann_appl_post_not_empty
%type <unionmemberlist> switch_body
%type <unionmember> case element_spec
%type <labellist> case_label case_labels
%type <typedeclist> type_declarator
%start specification
%%

/* (XTYP-116) <map_type> ::= "map" "<" <simple_type_spec> "," <ann_appl> <simple_type_spec> "," <ann_appl_post> <positive_int_const> ">"
                           | "map" "<" <simple_type_spec> "," <ann_appl> <simple_type_spec> <ann_appl_post> ">" */

map_type: T_KW_MAP T_P_O_A simple_type_spec T_P_COMMA ann_appl simple_type_spec T_P_COMMA ann_appl_post positive_int_const T_P_C_A { $$ = (Type *) create_map_type ($3, $6, $9); }
	| T_KW_MAP T_P_O_A simple_type_spec T_P_COMMA ann_appl simple_type_spec ann_appl_post T_P_C_A                              { $$ = (Type *) create_map_type ($3, $6, UNBOUNDED_COLLECTION); }
	;

/* (XTYP-115) <switch_type_name> ::= <integer_type>
                                   | <char_type>
                                   | <wide_char_type>
                                   | <boolean_type>
                                   | <enum_type>
                                   | <octet_type>
                                   | <scoped_name> */
switch_type_name: ann_appl integer_type     { $$=$2; }
                | ann_appl char_type        { $$=$2; }
                | ann_appl wide_char_type   { $$=$2; }
                | ann_appl boolean_type     { $$=$2; }
                | enum_type                 { $$=$1; }
                | ann_appl octet_type       { $$=$2; }
                | ann_appl scoped_name      { printf("OOPS: need to do type lookup\n"); exit(0); $$ =NULL; }
                ;

/* (XTYP-114) <struct_header> ::= <ann_appl> "struct" <identifier> [ ":" <scoped_name> ] */
struct_header_scoped_name_empty: /* Empty */		{ $$ = NULL; }
                               | T_P_COLON scoped_name  { $$ = (Type *) create_alias_type ($2, NULL); }
                               ;

struct_header: ann_appl T_KW_STRUCT T_IDENTIFIER struct_header_scoped_name_empty { $$=(Type *) create_structure_type($3, $4, NULL); }
             ;

/* (XTYP-113) <ann_appl_param> ::= <identifier> "=" <const_exp> */
ann_appl_param: T_IDENTIFIER T_P_IS const_exp
              ;

ann_appl_param_list: ann_appl_param
                   | ann_appl_param T_P_COMMA ann_appl_param_list
                   ;

/* (XTYP-112) <ann_appl_params> ::= <const_exp>
                                  | <ann_appl_param> { "," <ann_appl_param> }* */
ann_appl_params: const_exp
               | ann_appl_param_list
               ;

ann_appl_params_empty: /* Empty */
                     | ann_appl_params
                     ;

/* (XTYP-111) <ann_appl_dcl> ::= <annotation_name> [ "(" [ <ann_appl_params> ] ")" ] */
ann_appl_dcl_params_empty: /* Empty */
                         | T_P_O_R ann_appl_params_empty T_P_C_R
                         ;

ann_appl_dcl: annotation_name ann_appl_dcl_params_empty  { $$=create_annotation_usage ($1, NULL); /* TODO: Parameters */ }
            ;

/* (XTYP-110) <ann_appl_post> ::= { "//@" <ann_appl_dcl> }* */
ann_appl_post_not_empty: T_P_SLASHAT ann_appl_dcl 			  { $$=$2; }
                       | T_P_SLASHAT ann_appl_dcl ann_appl_post_not_empty { printf("implement multiple annotations\n"); exit(0); }
                       ;

ann_appl_post: /* Empty */			{ $$=NULL; }
             | ann_appl_post_not_empty T_EOL    { $$=$1; }
             ;

/* (XTYP-109) <ann_appl> ::= { "@" <ann_appl_dcl> }* */

ann_appl: /* Empty */
        | ann_appl T_P_AT ann_appl_dcl
        ;

/* (XTYP-108) <ann_attr> ::= <ann_appl> "attribute" <param_type_spec> <simple_declarator> [ "default" <const_exp> ] ";" <ann_appl_post> */
 
ann_attr_default: /* Empty */
                | T_KW_DEFAULT const_exp
                ;

ann_attr: ann_appl T_KW_ATTRIBUTE param_type_spec simple_declarator ann_attr_default T_P_SEMI ann_appl_post
        ;

ann_attr_list: ann_attr
             | ann_attr ann_attr_list
             ;

ann_attr_list_empty: /* Empty */
                   | ann_attr_list
                   ;

/* (XTYP-107) <annotation_name> ::= <scoped_name> */
annotation_name: scoped_name
               ;

/* (XTYP-106) <ann_inheritance_spec> ::= ":" <annotation_name> */
ann_inheritance_spec: T_P_COLON annotation_name
                    ;

ann_inheritance_spec_empty: /* Empty */
                          | ann_inheritance_spec
                          ;

/* (XTYP-105) <ann_body> ::= <ann_attr>* */
ann_body: ann_attr_list_empty
        ;

/* (XTYP-104) <ann_header> ::= "@annotation" [ "(" ")" ] "local" "interface" <identifier> [ <ann_inheritance_spec> ] 
 
   Added missing quotes above
 
 */

ann_header: T_KW_ANNOTATION T_KW_LOCAL T_KW_INTERFACE T_IDENTIFIER ann_inheritance_spec_empty
          | T_KW_ANNOTATION T_P_O_R T_P_C_R T_KW_LOCAL T_KW_INTERFACE T_IDENTIFIER ann_inheritance_spec_empty
          ;

/* (XTYP-103) <ann_fwd_dcl> ::= "@annotation" [ "(" ")" ] "local" "interface" <identifier> 
   
   Added missing quotes above

 */

ann_fwd_dcl: T_KW_ANNOTATION T_KW_LOCAL T_KW_INTERFACE T_IDENTIFIER
           | T_KW_ANNOTATION T_P_O_R T_P_C_R T_KW_LOCAL T_KW_INTERFACE T_IDENTIFIER
           ;

/* (XTYP-102) <ann_dcl> ::= <ann_header> "{" <ann_body> "}" */
ann_dcl: ann_header T_P_O_C ann_body T_P_C_C
       ;


/* (XTYP-101) <annotation> ::= <ann_dcl>
                             | <ann_fwd_dcl> */
annotation: ann_dcl
          | ann_fwd_dcl
          ;

/* (CORB-111) <exception_list>::= "(" <scoped_name> { "," <scoped_name> } * ")" */
exception_list: T_P_O_R scoped_name_list T_P_C_R
              ;

/* (CORB-110) <set_excep_expr> ::= "setraises" <exception_list> */
set_excep_expr: T_KW_SETRAISES exception_list
              ;

/* (CORB-109) <get_excep_expr> ::= "getraises" <exception_list> */
get_excep_expr: T_KW_GETRAISES exception_list
              ;

/* (CORB-108) <attr_raises_expr> ::=<get_excep_expr> [ <set_excep_expr> ]
                                  | <set_excep_expr> */
attr_raises_expr: get_excep_expr
                | get_excep_expr set_excep_expr
                | set_excep_expr
                ;

/* (CORB-107) <attr_declarator> ::=<simple_declarator> <attr_raises_expr>
                                 | <simple_declarator> { "," <simple_declarator> }* */
attr_declarator: simple_declarator attr_raises_expr
               | simple_declarator_list
               ;

/* (CORB-106) <attr_spec> ::= "attribute" <param_type_spec> <attr_declarator> */
attr_spec: T_KW_ATTRIBUTE param_type_spec attr_declarator
         ;

/* (CORB-105) <readonly_attr_declarator>::=<simple_declarator> <raises_expr>
                                         | <simple_declarator> { "," <simple_declarator> }* */
readonly_attr_declarator: simple_declarator raises_expr
                        | simple_declarator_list
                        ;

/* (CORB-104) <readonly_attr_spec> ::= "readonly" "attribute" <param_type_spec> <readonly_attr_declarator> */
readonly_attr_spec: T_KW_READONLY T_KW_ATTRIBUTE param_type_spec readonly_attr_declarator
                  ;

/* (CORB-103) <type_prefix_dcl>::="typeprefix" <scoped_name> <string_literal> */
type_prefix_dcl: T_KW_TYPEPREFIX scoped_name T_L_STRING
               ;

/* (CORB-102) <type_id_dcl> ::="typeid" <scoped_name> <string_literal> */
type_id_dcl: T_KW_TYPEID scoped_name T_L_STRING
           ;

/* (CORB-101) <imported_scope> ::= <scoped_name> | <string_literal> */
imported_scope: scoped_name
              | T_L_STRING
              ;

/* (CORB-100) <import> ::= "import" <imported_scope> ";" */
import: T_KW_IMPORT imported_scope T_P_SEMI
      ;

import_list: import
           | import import_list 
           ;

import_list_empty: /* Empty */
                 | import_list
                 ;

/* (CORB-99) <constr_forward_decl>::="struct" <identifier>
                                    | "union" <identifier> 

 Remark: We added ann_appl here to avoid loads of shift reduce conflicts. Treat
 is as valid grammar, but throw a syntax error when ann_appl is not NULL in a
 constr_forward_decl
 */
constr_forward_decl: ann_appl T_KW_STRUCT T_IDENTIFIER
                   | ann_appl T_KW_UNION T_IDENTIFIER
                   ;

/* (CORB-98) <value_base_type>::= "ValueBase" */
value_base_type: T_KW_VALUEBASE { $$ = create_invalid_type (); }
               ;

/* (CORB-97) <fixed_pt_const_type>::="fixed" */
fixed_pt_const_type: T_KW_FIXED
                   ;

/* (CORB-96) <fixed_pt_type>::="fixed" "<" <positive_int_const> "," <positive_int_const> ">" */
fixed_pt_type: T_KW_FIXED T_P_O_A positive_int_const T_P_COMMA positive_int_const T_P_C_A { $$ = create_invalid_type (); };
             ;

/* (CORB-95) <param_type_spec>::=<base_type_spec>
                               | <string_type>
                               | <wide_string_type>
                               | <scoped_name> */
param_type_spec: base_type_spec
               | string_type
               | wide_string_type
               | scoped_name
               ;

/* (CORB-94) <context_expr>::="context" "(" <string_literal> { "," <string_literal> }* ")" */
context_expr_list: T_L_STRING
                 | T_L_STRING T_P_COMMA context_expr_list
                 ;

context_expr: T_KW_CONTEXT T_P_O_R context_expr_list T_P_C_R
            ;

context_expr_empty: /* Empty */
                  | context_expr
                  ;

/* (CORB-93) <raises_expr>::="raises" "(" <scoped_name> { "," <scoped_name> }* ")" */

raises_expr: T_KW_RAISES T_P_O_R scoped_name_list T_P_C_R
           ;

raises_expr_empty: /* Empty */
                 | raises_expr
                 ;

/* (CORB-92) <param_attribute>::="in"
                          | "out"
                          | "inout" */
param_attribute: T_KW_IN
               | T_KW_OUT
               | T_KW_INOUT
               ;

/* (CORB-91) <param_dcl>::=<param_attribute> <param_type_spec> <simple_declarator> */
param_dcl: param_attribute param_type_spec simple_declarator
         ;

/* (CORB-90) <parameter_dcls>::="(" <param_dcl> { "," <param_dcl> }* ")"
                         | "(" ")" */
parameter_dcls_list: param_dcl
                   | param_dcl T_P_COMMA parameter_dcls_list
                   ;

parameter_dcls: T_P_O_R T_P_C_R
              | T_P_O_R parameter_dcls_list T_P_C_R
              ;

/* (CORB-89) <op_type_spec>::=<param_type_spec>
                       | "void" */
op_type_spec: param_type_spec
            | T_KW_VOID
            ;

/* (CORB-88) <op_attribute>::="oneway" */
op_attribute: /* Empty */
            | T_KW_ONEWAY
            ;

/* (CORB-87) <op_dcl>::=[ <op_attribute> ] <op_type_spec> <identifier> <parameter_dcls> [ <raises_expr> ] [ <context_expr> ] */
op_dcl: op_attribute op_type_spec T_IDENTIFIER parameter_dcls raises_expr_empty context_expr_empty
      ;

/* (CORB-86) <except_dcl>::="exception" <identifier> "{" <member>* "}" */
except_dcl: T_KW_EXCEPTION T_IDENTIFIER T_P_O_C member_list_empty T_P_C_C
          ;

/* (CORB-85) <attr_dcl> ::= <readonly_attr_spec>
                          | <attr_spec> */
attr_dcl: readonly_attr_spec
        | attr_spec
        ;

/* (CORB-84) <fixed_array_size>::="[" <positive_int_const> "]" */
fixed_array_size: T_P_O_S positive_int_const T_P_C_S	{ $$ = $2; }
                ;

fixed_array_sizes: fixed_array_size			{ $$=create_array_size($1); }
                 | fixed_array_sizes fixed_array_size   { $$=array_size_add($1, $2); }
                 ;

/* (CORB-83)  <array_declarator>::=<identifier>                            <fixed_array_size>+ */
/* (XTYP-212) <array_declarator>::=<identifier> <ann_appl> <ann_appl_post> <fixed_array_size>+ */
array_declarator: T_IDENTIFIER ann_appl ann_appl_post fixed_array_sizes { $$=create_array_declarator($1, $4); }
                ;

/* (CORB-82) <wide_string_type>::="wstring" "<" <positive_int_const> ">"
                                | "wstring" */
wide_string_type: T_KW_WSTRING T_P_O_A positive_int_const T_P_C_A { $$ = create_invalid_type (); }
                | T_KW_WSTRING                                    { $$ = create_invalid_type (); }
                ;

/* (CORB-81) <string_type>::="string" "<" <positive_int_const> ">"
                      | "string" */
string_type: T_KW_STRING T_P_O_A positive_int_const T_P_C_A       { $$ = (Type *) create_string_type ($3); }
           | T_KW_STRING                                          { $$ = (Type *) create_string_type (UNBOUNDED_COLLECTION); }
           ;

/* (CORB-80)  <sequence_type>::="sequence" "<"            <simple_type_spec> ","                 <positive_int_const> ">"
                              | "sequence" "<"            <simple_type_spec>                 ">" */
/* (XTYP-211) <sequence_type>::="sequence" "<" <ann_appl> <simple_type_spec> "," <ann_appl_post> <positive_int_const> ">"
                              | "sequence" "<" <ann_appl> <simple_type_spec> <ann_appl_post> ">" */
sequence_type: T_KW_SEQUENCE T_P_O_A ann_appl simple_type_spec T_P_COMMA ann_appl_post positive_int_const T_P_C_A { $$ = (Type *) create_sequence_type ($4, $7); }
             | T_KW_SEQUENCE T_P_O_A ann_appl simple_type_spec ann_appl_post T_P_C_A                              { $$ = (Type *) create_sequence_type ($4, UNBOUNDED_COLLECTION); }
             ;

/* (CORB-79)  <enumerator>::=<identifier> */
/* (XTYP-206) <enumerator>::=<ann_appl> ... */
enumerator: ann_appl T_IDENTIFIER { $$=create_enum_const($2); }
          ;

enumerators: enumerator ann_appl_post { $$=create_enum_const_list($1); }
           | enumerator T_P_COMMA ann_appl_post enumerators { $$=enum_const_list_prepend($4, $1); }
           ;
 
/* (CORB-78)  <enum_type>::="enum" <identifier> "{" <enumerator> { "," <enumerator> }* "}" */
/* (XTYP-210) <enum_type> ::= <ann_appl> "enum" <identifier> "{" <enumerator> { "," <ann_appl_post> <enumerator> }* <ann_appl_post> "}" */
enum_type: ann_appl T_KW_ENUM T_IDENTIFIER T_P_O_C enumerators T_P_C_C  { $$= (Type *) create_enumeration_type ($3, $5); }
         ;

/* (CORB-77)  <element_spec>::=<type_spec> <declarator> */
/* (XTYP-205) <element_spec>::=<ann_appl> ... 

   Why add an anotation here: the type_spec is already annotated?
 
 */
element_spec: type_spec declarator { $$=create_union_member($1,$2); }
            ;

/* (CORB-76) <case_label>::="case" <const_exp> ":"
                    | "default" ":" */
/* TODO: case_label: T_KW_CASE const_exp T_P_COLON { $$=create_label_list(); } */
case_label: T_KW_CASE literal T_P_COLON   { $$=create_label_list(literal_to_uint32(&$2)); }
          | T_KW_DEFAULT T_P_COLON        { $$=create_label_list_default(); }
          ;

case_labels: case_label			   { $$=$1; }
           | case_label case_labels	   { $$=label_list_prepend($2, $1); }
           ;

/* (CORB-75)  <case>::=<case_label>+ <element_spec> ";" */
/* (XTYP-204) <case> ::= ... <ann_appl_post> */
case: case_labels element_spec T_P_SEMI ann_appl_post { $$=union_member_add_labels($2, $1); }
    ;

/* (CORB-74) <switch_body>::=<case>+ */
switch_body: case		{ $$=create_union_member_list($1); }
           | switch_body case   { $$=union_member_list_prepend ($1, $2); }
           ;

/* (CORB-73)  <switch_type_spec>::=<integer_type>
                                 | <char_type>
                                 | <boolean_type>
                                 | <enum_type>
                                 | <scoped_name> */
/* (XTYP-202) <switch_type_spec> ::= <ann_appl> <switch_type_name> <ann_appl_post> */
/* (XTYP-209) <switch_type_spec> ::= <ann_appl> <switch_type_name> <ann_appl_post> */
/* Switch type name is already annotated */
switch_type_spec: switch_type_name ann_appl_post
                ;

/* (CORB-72)  <union_type>::="union" <identifier> "switch" "(" <switch_type_spec> ")" "{" <switch_body> "}" */
/* (XTYP-201) <union_type> ::= <ann_appl> ... */
union_type: ann_appl T_KW_UNION T_IDENTIFIER T_KW_SWITCH T_P_O_R switch_type_spec T_P_C_R T_P_O_C switch_body T_P_C_C { $$ = (Type *) create_union_type ($3, $6, $9); }
          ;

/* (CORB-71)  <member>::=<type_spec> <declarators> ";" */
/* (XTYP-203) <member>::= ...
                        | <ann_appl> <type_spec> <declarator> ";" <ann_appl_post> */

multiple_member_declarators: declarator declarator		    { $$=declarator_list_prepend($1, $2); }
                           | multiple_member_declarators declarator { $$=declarator_list_prepend($1, $2); }
                           ;

member_declarator: multiple_member_declarators T_P_SEMI { $$=$1; }
                 | declarator T_P_SEMI ann_appl_post    { $$=declarator_list_add_annotation($1,$3); }
                 ;

member: type_spec member_declarator { $$ = member_list_from_declarator_list ($1, $2); }
      ;

/* (CORB-70) <member_list>::=<member>+ */

member_list: member              { $$=$1; }
           | member member_list  { $$=member_list_join($2, $1); }
           ;

member_list_empty: /* Empty */
                 | member_list
                 ;

/* (CORB-69)  <struct_type>::="struct" <identifier> "{" <member_list> "}" */
/* (XTYP-208) <struct_type>::=<struct_header>       "{" <member_list> "}" */
struct_type: struct_header T_P_O_C member_list T_P_C_C {  $$=(Type *) struct_set_members((StructureType *) $1, $3); }
           ;

/* (CORB-68) <object_type>::="Object" */
object_type: T_KW_OBJECT { $$ = create_invalid_type (); }
           ;

/* (CORB-67) <any_type>::="any" */
any_type: T_KW_ANY { $$ = create_invalid_type (); }
        ;

/* (CORB-66) <octet_type>::="octet" */
octet_type: T_KW_OCTET { $$ = get_primitive_type (BYTE_TYPE); }
          ;

/* (CORB-65) <boolean_type>::="boolean" */
boolean_type: T_KW_BOOLEAN { $$ = get_primitive_type (BOOLEAN_TYPE); }
            ;

/* (CORB-64) <wide_char_type>::="wchar" */
wide_char_type: T_KW_WCHAR { $$ = get_primitive_type (CHAR_32_TYPE); }
              ;

/* (CORB-63) <char_type>::="char" */
char_type: T_KW_CHAR { $$ = get_primitive_type (CHAR_8_TYPE); }
         ;

/* (CORB-62) <unsigned_longlong_int>::="unsigned" "long" "long" */ 
unsigned_longlong_int: T_KW_UNSIGNED T_KW_LONG T_KW_LONG { $$ = get_primitive_type (UINT_64_TYPE); }
                     ;

/* (CORB-61) <unsigned_long_int>::="unsigned" "long" */
unsigned_long_int: T_KW_UNSIGNED T_KW_LONG { $$ = get_primitive_type (UINT_32_TYPE); }
                 ;

/* (CORB-60) <unsigned_short_int>::="unsigned" "short" */
unsigned_short_int: T_KW_UNSIGNED T_KW_SHORT { $$ = get_primitive_type (UINT_16_TYPE); }
                  ;

/* (CORB-59) <unsigned_int>::=<unsigned_short_int>
                      | <unsigned_long_int>
                      | <unsigned_longlong_int> */
unsigned_int: unsigned_short_int       { $$=$1; }
            | unsigned_long_int        { $$=$1; }
            | unsigned_longlong_int    { $$=$1; }
            ;

/* (CORB-58) <signed_longlong_int>::="long" "long" */
signed_longlong_int: T_KW_LONG T_KW_LONG { $$ = get_primitive_type (INT_64_TYPE); }
                   ;

/* (CORB-57) <signed_long_int>::="long" */
signed_long_int: T_KW_LONG { $$ = get_primitive_type (INT_32_TYPE); }
               ;

/* (CORB-56) <signed_short_int>::="short" */
signed_short_int: T_KW_SHORT { $$ = get_primitive_type (INT_16_TYPE); }
                ;

/* (CORB-55) <signed_int>::=<signed_short_int>
                          | <signed_long_int>
                          | <signed_longlong_int> */
signed_int: signed_short_int    { $$=$1; }
          | signed_long_int     { $$=$1; }
          | signed_longlong_int { $$=$1; }
          ;

/* (CORB-54) <integer_type>::=<signed_int>
                            | <unsigned_int> */
integer_type: signed_int      { $$=$1; }
            | unsigned_int    { $$=$1; }
            ;

/* (CORB-53) <floating_pt_type>::="float"
                                | "double"
                                | "long" "double" */

floating_pt_type: T_KW_FLOAT              { $$ = get_primitive_type (FLOAT_32_TYPE); }
                | T_KW_DOUBLE             { $$ = get_primitive_type (FLOAT_64_TYPE); }
                | T_KW_LONG T_KW_DOUBLE   { $$ = get_primitive_type (FLOAT_128_TYPE); }
                ; 

/* (CORB-52) <complex_declarator>::=<array_declarator> */
complex_declarator: array_declarator { $$=$1; }
                  ;

/* (CORB-51) <simple_declarator>::=<identifier> */
simple_declarator: T_IDENTIFIER	{ $$=create_simple_declarator($1); }	
                 ;

simple_declarator_list: simple_declarator 
                      | simple_declarator simple_declarator_list
                      ;

/* (CORB-50) <declarator>::=<simple_declarator>
                     | <complex_declarator> */
declarator: simple_declarator		{ $$=$1; }
          | complex_declarator		{ $$=$1; }
          ;

/* (CORB-49) <declarators>::=<declarator> { "," <declarator> }* */
declarators: declarator                         { $$=$1; }
           | declarators T_P_COMMA declarator   { $$=declarator_list_prepend($1,$3); }
           ;

/* (CORB-48) <constr_type_spec>::=<struct_type>
                          | <union_type>
                          | <enum_type> */
constr_type_spec: struct_type { $$=$1; }
                | union_type  { $$=$1; }
                | enum_type   { $$=$1; }
                ;

/* (CORB-47)  <template_type_spec>::=<sequence_type>
                                   | <string_type>
                                   | <wide_string_type>
                                   | <fixed_pt_type> */
/* (XTYP-207) <template_type_spec> ::= ... | <map_type> */
template_type_spec: sequence_type       { $$ = (Type *) $1; }
                  | string_type         { $$ = (Type *) $1; }
                  | wide_string_type    { $$ = (Type *) $1; }
                  | fixed_pt_type       { $$ = (Type *) $1; }
                  | map_type            { $$ = (Type *) $1; }
                  ;

/* (CORB-46) <base_type_spec>::=<floating_pt_type>
                              | <integer_type>
                              | <char_type>
                              | <wide_char_type>
                              | <boolean_type>
                              | <octet_type>
                              | <any_type>
                              | <object_type>
                              | <value_base_type> */
base_type_spec: floating_pt_type { $$=$1; }
              | integer_type     { $$=$1; } 
              | char_type        { $$=$1; }
              | wide_char_type   { $$=$1; }
              | boolean_type     { $$=$1; }
              | octet_type       { $$=$1; }
              | any_type         { $$=$1; }
              | object_type      { $$=$1; }
              | value_base_type  { $$=$1; }
              ;

/* (CORB-45) <simple_type_spec>::=<base_type_spec>
                                | <template_type_spec>
                                | <scoped_name> */

simple_type_spec: base_type_spec     { $$ = $1; }
                | template_type_spec { $$ = $1; }
                | scoped_name        { $$ = lookup_type ($1); if ($$ == NULL) { idlerror("Unknown type"); } } 
                ;

/* (CORB-44) <type_spec>::=<simple_type_spec>
                         | <constr_type_spec> */

type_spec: ann_appl simple_type_spec { $$=$2; }
         | constr_type_spec          { $$=$1; }
         ;

/* (CORB-43) <type_declarator>::=<type_spec> <declarators> */
type_declarator: type_spec declarators  { $$=declarator_list_set_type($2, $1);  }
               ;

/* (CORB-42) <type_dcl>::="typedef" <type_declarator>
                   | <struct_type>
                   | <union_type>
                   | <enum_type>
                   | "native" <simple_declarator>
                   | <constr_forward_decl> */

type_dcl: T_KW_TYPEDEF type_declarator { register_alias_types_from_declarator_list($2); }
        | struct_type                  { register_type($1); } 
        | union_type                   { register_type($1); }
        | enum_type                    { register_type($1); }
        | T_KW_NATIVE simple_declarator
        | constr_forward_decl
        ;

/* (CORB-41) <positive_int_const>::=<const_exp> */
/* TODO: positive_int_const: const_exp { printf("TODO evaluate constants\n"); $$=0; $$=evaluate_const_exp($1); } */
positive_int_const: literal { if ($1.kind == INTEGER_KIND) $$=atoi($1.string); }
                  ;

/* (CORB-40) <boolean_literal>::="TRUE"
                          | "FALSE" */

boolean_literal: T_KW_TRUE	{ $$.kind=BOOLEAN_KIND; $$.string=strdup("1"); }
               | T_KW_FALSE	{ $$.kind=BOOLEAN_KIND; $$.string=strdup("0"); }
               ;

/* (CORB-39) <literal>::=<integer_literal>
                  | <string_literal>
                  | <wide_string_literal>
                  | <character_literal>
                  | <wide_character_literal>
                  | <fixed_pt_literal>
                  | <floating_pt_literal>
                  | <boolean_literal> */
literal: T_L_INTEGER        { $$=$1; }
       | T_L_STRING         { $$=$1; }
       | T_L_WSTRING        { $$=$1; }
       | T_L_CHAR           { $$=$1; }
       | T_L_WCHAR          { $$=$1; }
       | T_L_FIXED          { $$=$1; }
       | T_L_FLOAT          { $$=$1; }
       | boolean_literal    { $$=$1; }
       ;


/* (CORB-38) <primary_expr>::=<scoped_name>
                       | <literal>
                       | "(" <const_exp> ")" */
primary_expr: scoped_name
            | literal
            | T_P_O_R const_exp T_P_C_R
            ;

/* (CORB-37) <unary_operator>::="-"
                         | "+"
                         | "~" */
unary_operator: T_P_MIN
              | T_P_PLUS
              | T_P_TILDE
              ;

/* (CORB-36) <unary_expr>::=<unary_operator> <primary_expr>
                     | <primary_expr> */
unary_expr: unary_operator primary_expr
          | primary_expr
          ;

/* (CORB-35) <mult_expr>::=<unary_expr>
                    | <mult_expr> "*" <unary_expr>
                    | <mult_expr> "/" <unary_expr>
                    | <mult_expr> "%" <unary_expr> */
mult_expr: unary_expr
         | mult_expr T_P_STAR unary_expr
         | mult_expr T_P_SLASH unary_expr
         | mult_expr T_P_PERCENT unary_expr 
         ;

/* (CORB-34) <add_expr>::=<mult_expr>
                  | <add_expr> "+" <mult_expr>
                  | <add_expr> "-" <mult_expr> */
add_expr: mult_expr
        | add_expr T_P_PLUS mult_expr
        | add_expr T_P_MIN mult_expr
        ;

/* (CORB-33) <shift_expr>::=<add_expr>
                    | <shift_expr> ">>" <add_expr>
                    | <shift_expr> "<<" <add_expr> */
shift_expr: add_expr
          | shift_expr T_P_SHIFT_R add_expr
          | shift_expr T_P_SHIFT_L add_expr
          ;

/* (CORB-32) <and_expr>::=<shift_expr>
                   | <and_expr> "&" <shift_expr> */
and_expr: shift_expr
        | and_expr T_P_AND shift_expr
        ;

/* (CORB-31) <xor_expr>::=<and_expr>
                   | <xor_expr> "^" <and_expr> */
xor_expr: and_expr
        | xor_expr T_P_CIRC and_expr
        ;

/* (CORB-30) <or_expr>::=<xor_expr>
                  | <or_expr> "|" <xor_expr> */

or_expr: xor_expr
       | or_expr T_P_PIPE xor_expr
       ;

/* (CORB-29) <const_exp>::=<or_expr> */
const_exp: or_expr
         ;

/* (CORB-28) <const_type>::=<integer_type>
                     | <char_type>
                     | <wide_char_type>
                     | <boolean_type>
                     | <floating_pt_type>
                     | <string_type>
                     | <wide_string_type>
                     | <fixed_pt_const_type>
                     | <scoped_name>
                     | <octet_type> */
const_type: integer_type
          | char_type
          | wide_char_type
          | boolean_type
          | floating_pt_type
          | string_type
          | wide_string_type
          | fixed_pt_const_type
          | scoped_name
          | octet_type
          ;

/* (CORB-27) <const_dcl>::="const" <const_type> <identifier> "=" <const_exp> */
const_dcl: T_KW_CONST const_type T_IDENTIFIER T_P_IS const_exp
         ;

/* (CORB-26) <init_param_attribute> ::="in" */
init_param_attribute: T_KW_IN
                    ;

/* (CORB-25) <init_param_decl> ::=<init_param_attribute> <param_type_spec> <simple_declarator> */
init_param_decl: init_param_attribute param_type_spec simple_declarator
               ;

/* (CORB-24) <init_param_decls> ::=<init_param_decl> { "," <init_param_decl> }* */
init_param_decls: init_param_decl
                | init_param_decl T_P_COMMA init_param_decl
                ;

init_param_decls_empty: /*Empty */
                      | init_param_decls
                      ;

/* (CORB-23) <init_dcl> ::="factory" <identifier> "(" [ <init_param_decls> ] ")" [ <raises_expr> ] ";" */
init_dcl: T_KW_FACTORY T_IDENTIFIER T_P_O_R init_param_decls_empty T_P_C_R raises_expr_empty T_P_SEMI
        ;

/* (CORB-22) <state_member> ::=( "public" | "private" ) <type_spec> <declarators> ";" */
state_member_modifier: T_KW_PUBLIC
                     | T_KW_PRIVATE
                     ;

state_member: state_member_modifier type_spec declarators T_P_SEMI
            ;

/* (CORB-21) <value_element> ::=<export> | < state_member> | <init_dcl> */
value_element: export
             | state_member
             | init_dcl
             ;

value_element_list: value_element
                  | value_element value_element_list
                  ;

value_element_list_empty: /* Empty */
                        | value_element_list
                        ;

/* (CORB-20) <value_name> ::=<scoped_name> */
value_name: scoped_name
          ;

value_name_list: value_name
               | value_name T_P_COMMA value_name_list
               ;

/* (CORB-19) <value_inheritance_spec> ::=[ ":" [ "truncatable" ] <value_name> { "," <value_name> }* ] [ "supports" <interface_name> { "," <interface_name> }* ] */
value_inheritance_spec_modifiers: /* Empty*/
                                | T_KW_TRUNCATABLE
                                ;

value_inheritance_spec_supports: /* Empty */
                               | T_KW_SUPPORTS interface_name_list
                               ;

value_inheritance_spec: /* Empty */
                      | T_P_COLON value_inheritance_spec_modifiers value_name_list value_inheritance_spec_supports
                      ;

valuetype_with_id: T_KW_VALUETYPE T_IDENTIFIER
                 ;

/* (CORB-18) <value_header> ::=["custom" ] "valuetype" <identifier> [ <value_inheritance_spec> ] */
value_header: valuetype_with_id value_inheritance_spec
            | T_KW_CUSTOM valuetype_with_id value_inheritance_spec
            ;

/* (CORB-17) <value_dcl> ::=<value_header> "{" <value_element>* "}" */
value_dcl: value_header T_P_O_C value_element_list_empty T_P_C_C
         ;

/* (CORB-16) <value_abs_dcl> ::="abstract" "valuetype" <identifier> [ <value_inheritance_spec> ] "{" <export>* "}" */
value_abs_dcl: T_KW_ABSTRACT valuetype_with_id value_inheritance_spec T_P_O_C export_list_empty T_P_C_C
             ;

/* (CORB-15) <value_box_dcl> ::="valuetype" <identifier> <type_spec> */
value_box_dcl: valuetype_with_id type_spec
             ;

/* (CORB-14) <value_forward_dcl> ::=[ "abstract" ] "valuetype" <identifier> */
value_forward_dcl: valuetype_with_id
                 | T_KW_ABSTRACT valuetype_with_id
                 ;

/* (CORB-13) <value> ::= ( <value_dcl> | <value_abs_dcl> | <value_box_dcl> | <value_forward_dcl>) */
value: value_dcl
     | value_abs_dcl
     | value_box_dcl
     | value_forward_dcl
     ;

/* (CORB-12) <scoped_name>::=<identifier>
                           | "::" <identifier>
                           | <scoped_name> "::" <identifier> */
scoped_name: T_IDENTIFIER				{ $$=$1; }
           | T_P_DOUBLE_COLON T_IDENTIFIER              { $$=str_append_free_2 ("::", $2); }
           | scoped_name T_P_DOUBLE_COLON T_IDENTIFIER  { $$=str_append3_free_13($1, "::",   $3); }
           ;


scoped_name_list: scoped_name
                | scoped_name T_P_COMMA scoped_name_list
                ;


/* (CORB-11) <interface_name>::=<scoped_name> */
interface_name: scoped_name
              ;

interface_name_list: interface_name
                   | interface_name T_P_COMMA interface_name_list
                   ;

/* (CORB-10) <interface_inheritance_spec>::=":" <interface_name> { "," <interface_name> }* */
interface_inheritance_spec_list: interface_name 
                               | interface_name T_P_COMMA interface_inheritance_spec_list
                               ;

interface_inheritance_spec: /* Empty */
                          | T_P_COLON interface_inheritance_spec_list
                          ;

/* (CORB-9) <export>::=<type_dcl> ";"
                    | <const_dcl> ";"
                    | <except_dcl> ";"
                    | <attr_dcl> ";"
                    | <op_dcl> ";"
                    | <type_id_dcl> ";"
                    | <type_prefix_dcl> ";" */
export: type_dcl T_P_SEMI
      | const_dcl T_P_SEMI
      | except_dcl T_P_SEMI
      | attr_dcl T_P_SEMI
      | op_dcl T_P_SEMI
      | type_id_dcl T_P_SEMI
      | type_prefix_dcl T_P_SEMI
      ;

export_list: export
           | export export_list
           ;

export_list_empty: /* Empty */
                 | export_list
                 ;

/* (CORB-8) <interface_body>::=<export>* */
interface_body: /* Empty */
              | export interface_body
              ;

interface_modifier: /* Empty */ 
                  | T_KW_ABSTRACT
                  | T_KW_LOCAL
                  ;

/* (CORB-7) <interface_header>::=[ "abstract" | "local" ] "interface" <identifier> [ <interface_inheritance_spec> ] */
interface_header: interface_modifier T_KW_INTERFACE T_IDENTIFIER interface_inheritance_spec
                ;

/* (CORB-6) <forward_dcl>::=[ "abstract" | "local" ] "interface" <identifier> */
forward_dcl: interface_modifier T_KW_INTERFACE T_IDENTIFIER
           ;

/* (CORB-5) <interface_dcl>::=<interface_header> "{" <interface_body> "}" */
interface_dcl: interface_header T_P_O_C interface_body T_P_C_C
             ;

/* (CORB-4) <interface>::=<interface_dcl>
                   | <forward_dcl> */
interface: interface_dcl
         | forward_dcl
         ;

/* (CORB-3) <module>::="module" <identifier> "{" <definition>+ "}" */
module: T_KW_MODULE T_IDENTIFIER T_P_O_C definition_list T_P_C_C
      ;

/* (CORB-2) <definition>::=<type_dcl> ";"
                     | <const_dcl> ";"
                     | <except_dcl> ";"
                     | <interface> ";"
                     | <module> ";"
                     | <value> ";"
                     | <type_id_dcl> ";"
                     | <type_prefix_dcl> ";"
                     | <event> ";"
                     | <component> ";"
                     | <home_dcl> ";" */
/* (XTYP-213) <definition> ::= <type_dcl> ";" <ann_appl_post>
                             | ...
                             | <annotation> ";" <ann_appl_post> */
definition: type_dcl T_P_SEMI ann_appl_post
          | const_dcl T_P_SEMI
          | except_dcl T_P_SEMI
          | interface T_P_SEMI
          | module T_P_SEMI
          | value T_P_SEMI
          | type_id_dcl T_P_SEMI
          | type_prefix_dcl T_P_SEMI
          /*| event T_P_SEMI
          | component T_P_SEMI
          | home_dcl T_P_SEMI */
          | annotation T_P_SEMI ann_appl_post
          ;

definition_list: definition
               | definition_list definition
               ;

/* (CORB-1) <specification>::=<import>* <definition>+ */
specification: import_list_empty definition_list { }
             ;

/*
NOTE: Grammar rules 1 through 111 with the exception of the last three lines of rule 2 constitutes the portion of IDL that
is not related to components.
(CORB-112) <component> ::=<component_dcl>
| <component_forward_dcl>
(CORB-113) <component_forward_dcl> ::= "component" <identifier>
(CORB-114) <component_dcl> ::= <component_header>
"{" <component_body> "}"
(CORB-115) <component_header> ::= "component" <identifier>
[ <component_inheritance_spec> ]
[ <supported_interface_spec> ]
(CORB-116) <supported_interface_spec> ::= "supports" <scoped_name>
{ "," <scoped_name> }*
(CORB-117) <component_inheritance_spec> ::= ":" <scoped_name>
(CORB-118) <component_body> ::=<component_export>*
(CORB-119) <component_export> ::=<provides_dcl> ";"
| <uses_dcl> ";"
| <emits_dcl> ";"
| <publishes_dcl> ";"
| <consumes_dcl> ";"
| <attr_dcl> ";"
(CORB-120) <provides_dcl> ::= "provides" <interface_type> <identifier>
(CORB-121) <interface_type> ::= <scoped_name>
| "Object"
(CORB-122) <uses_dcl> ::= "uses" [ "multiple" ]
< interface_type> <identifier>
(CORB-123) <emits_dcl> ::= "emits" <scoped_name> <identifier>
(CORB-124) <publishes_dcl> ::= "publishes" <scoped_name> <identifier>
(CORB-125) <consumes_dcl> ::= "consumes" <scoped_name> <identifier>
(CORB-126) <home_dcl> ::= <home_header> <home_body>
(CORB-127) <home_header> ::= "home" <identifier>
[ <home_inheritance_spec> ]
[ <supported_interface_spec> ]
"manages" <scoped_name>
[ <primary_key_spec> ]
(CORB-128) <home_inheritance_spec> ::= ":" <scoped_name>
(CORB-129) <primary_key_spec> ::= "primarykey" <scoped_name>
(CORB-130) <home_body> ::= "{" <home_export>* "}"
(CORB-131) <home_export ::= <export>
| <factory_dcl> ";"
| <finder_dcl> ";"
(CORB-132) <factory_dcl> ::= "factory" <identifier>
"(" [ <init_param_decls> ] ")"
[ <raises_expr> ]
(CORB-133) <finder_dcl> ::= "finder" <identifier>
"(" [ <init_param_decls> ] ")"
[ <raises_expr> ]
(CORB-134) <event> ::= ( <event_dcl> | <event_abs_dcl> |
                <event_forward_dcl>)
(CORB-135) <event_forward_dcl> ::=[ "abstract" ] "eventtype" <identifier>
(CORB-136) <event_abs_dcl> ::="abstract" "eventtype" <identifie
[ <value_inheritance_spec> ]
"{" <export>* "}"
(CORB-137) <event_dcl>::=<event_header> "{" <value_element> * "}"
(CORB-138) <event_header>::=[ "custom" ] "eventtype" <identifier> [ <value_inheritance_spec> ] */

%%
