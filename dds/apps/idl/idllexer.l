        /* Definitions section */

        /* Verbatim top copied block */
%top{
        /* This goes at the "top" of the generated file. */
#include "typeobj.h"
#include "idlparser.h"
}


        /* Verbatim copied block */
%{
        static int lineno = 1;
        static int charno = 1;
        static int saw_slash_at = 0;
        #define MVCHARNO(str)   charno += strlen(str)
%}

        /* Definitions:*/
        /*      Alphabetic characters - Table 7.2. 
         *      TODO: Need to add the other alphabetic characters (with
         *      accents) when I figure out how to do that in a portable
         *      fashion. */
ASCIIAlphabeticCharacter        [a-zA-Z]
AlphabeticCharacter             ({ASCIIAlphabeticCharacter})
        /*      Decimal Digits - Table 7.3. */
DecimalDigitNotNull             [1-9]
DecimalDigitNull                "0"
DecimalDigit                    ({DecimalDigitNotNull}|{DecimalDigitNull})
        /*      Graphic Characters - Table 7.4. 
         *      TODO: 
         *      inverted exclamation mark
         *      cent sign
         *      pound sign
         *      currency sign
         *      yen sign
         *      broken bar
         *      section/paragraph sign
         *      diaeresis
         *      copyright sign
         *      feminine ordinal indicator
         *      left angle quotation mark
         *      not sign
         *      soft hyphen
         *      registered trade mark sign
         *      macron
         *      ring above, degree sign
         *      plus-minus sign
         *      superscript two
         *      superscript three
         *      pilcrow
         *      middle dot
         *      cedilla
         *      superscript one
         *      masculine ordinal indicator
         *      right angle quotation mark
         *      vulgar fraction 1/4
         *      vulgar fraction 1/2
         *      vulgar fraction 3/4
         *      inverted question mark
         *      multiplication sign
         *      division sign 
         *      apostroph / acute (don't know which one) */
Underscore                      _
OtherGraphicCharacter           [!"#$%&'()\*\+,\-./:;<=>?@\[\\\]\^`{|}~]
GraphicCharacter                ({Underscore}|{OtherGraphicCharacter}) 
        /*      Formatting Characters - Table 7.5. 
         *      TODO */
FormattingCharacter             [\007\010\011\n\013\014\r]       
Space                           [ ]

        /*      Identifiers - Section 7.2.3. */
Identifier                      {ASCIIAlphabeticCharacter}({ASCIIAlphabeticCharacter}|{DecimalDigit}|{Underscore})*
        /*      Escaped Identifiers - Section 7.2.3.1 */
EscapedIdentifier               {Underscore}{Identifier}


        /* Literals - Section 7.2.5. */
        /* Integer Literals - Section 7.2.5.1. */
OctalDigit                      [0-7]
HexadecimalDigit                [0-9a-fA-F]
DecimalLiteral                  ({DecimalDigitNull}|{DecimalDigitNotNull}{DecimalDigit}*)
OctalLiteral                    "0"{OctalDigit}+
HexadecimalLiteral              "0"[xX]{HexadecimalDigit}+
IntegerLiteral                  ({OctalLiteral}|{HexadecimalLiteral}|{DecimalLiteral})
        /* Char Literals - Section 7.2.5.2. */
        /* Escapes - Table 7.9 */
CharacterEscape1                "\\"[ntvbrfa\\?'"]
CharacterEscape2                "\\"{OctalDigit}{1,3}
CharacterEscape3                "\\x"{HexadecimalDigit}{1,2}
CharacterEscape4                "\\u"{HexadecimalDigit}{1,4}
CharacterEscape                 ({CharacterEscape1}|{CharacterEscape2}|{CharacterEscape3})
CharacterEscapeWide             ({CharacterEscape1}|{CharacterEscape2}|{CharacterEscape3}|{CharacterEscape4})
Character                       ({AlphabeticCharacter}|{DecimalDigit}|{GraphicCharacter}|{Space}|{CharacterEscape})
CharacterWide                   ({AlphabeticCharacter}|{DecimalDigit}|{GraphicCharacter}|{Space}|{CharacterEscapeWide})
CharacterLiteral                "'"{Character}"'"
        /* Floating Point Literal - Section 7.2.5.3 */
FloatingPointDecimalPart        {DecimalDigit}+
FloatingPointFractionPart       {DecimalDigit}+
FloatingPointExponent           [eE]("+"|"-")?{DecimalDigit}+

FloatingPointLiteral1           {FloatingPointDecimalPart}"."{FloatingPointFractionPart}?{FloatingPointExponent}?
FloatingPointLiteral2           "."{FloatingPointFractionPart}{FloatingPointExponent}?
FloatingPointLiteral3           {FloatingPointDecimalPart}{FloatingPointExponent}

FloatingPointLiteral            ({FloatingPointLiteral1}|{FloatingPointLiteral2}|{FloatingPointLiteral3})

        /* String Literal - Section 7.2.5.4. 
         * 
         * WARNING: this allows \0 in strings!!!!*/
StringLiteral                   "\""{Character}*"\""



        /* Modes */
%x mlcomment

%option nounput
%option noinput


%%
        /* Rules section */
"//@"				{ charno += 3;              saw_slash_at = 1; return T_P_SLASHAT; }
        /* Comments*/
"/*"                            { charno += 2;              BEGIN(mlcomment); }
"//"[^@].*\n                    { charno  = 1;              lineno++; if (saw_slash_at) { saw_slash_at = 0; return T_EOL; } }
<mlcomment>[^*\n]*              { charno += strlen(yytext);                   } /* eat anything that's not a '*' */
<mlcomment>"*"+[^*/\n]*         { charno += strlen(yytext);                   } /* eat up '*'s not followed by '/'s */
<mlcomment>\n                   { charno  = 1;              lineno++; }
<mlcomment>"*"+"/"              { charno += strlen(yytext); BEGIN(INITIAL);   }


        /* Whitespace handling */
[ \t]                           { charno ++; } 
        /* Ignore carriage return */
\r                              { charno ++; }
        /* Newline handling */
\n                              { lineno ++; charno = 1; if (saw_slash_at) { saw_slash_at = 0; return T_EOL; } }

        /* Punctuation */
";"                             { charno ++;   return T_P_SEMI; }
"{"                             { charno ++;   return T_P_O_C; }
"}"                             { charno ++;   return T_P_C_C; }
"::"                            { charno += 2; return T_P_DOUBLE_COLON; }
":"                             { charno ++;   return T_P_COLON; }
","                             { charno ++;   return T_P_COMMA; }
"="                             { charno ++;   return T_P_IS; }
"+"                             { charno ++;   return T_P_PLUS; }
"-"                             { charno ++;   return T_P_MIN; }
"("                             { charno ++;   return T_P_O_R; }
")"                             { charno ++;   return T_P_C_R; }
"<<"                            { charno += 2; return T_P_SHIFT_L; }
">>"                            { charno += 2; return T_P_SHIFT_R; }
"<"                             { charno ++;   return T_P_O_A; }
">"                             { charno ++;   return T_P_C_A; }
"["                             { charno ++;   return T_P_O_S; }
"]"                             { charno ++;   return T_P_C_S; }
"\\"                            { charno ++;   return T_P_BACK; }
"|"                             { charno ++;   return T_P_PIPE; }
"^"                             { charno ++;   return T_P_CIRC; }
"&"                             { charno ++;   return T_P_AND; }
"*"                             { charno ++;   return T_P_STAR; }
"/"                             { charno ++;   return T_P_SLASH; }
"%"                             { charno ++;   return T_P_PERCENT; }
"~"                             { charno ++;   return T_P_TILDE; }
      /* Needed for xtypes */
"@Annotation"                   { MVCHARNO(yytext); return T_KW_ANNOTATION; }
"@"                             { charno ++;        return T_P_AT; }

{EscapedIdentifier}             { MVCHARNO(yytext); return T_IDENTIFIER; }

        /* Keywords */
abstract                        { MVCHARNO(yytext); return T_KW_ABSTRACT; }
exception                       { MVCHARNO(yytext); return T_KW_EXCEPTION; }
inout                           { MVCHARNO(yytext); return T_KW_INOUT; }
provides                        { MVCHARNO(yytext); return T_KW_PROVIDES; }
truncatable                     { MVCHARNO(yytext); return T_KW_TRUNCATABLE; }
any                             { MVCHARNO(yytext); return T_KW_ANY; }
emits                           { MVCHARNO(yytext); return T_KW_EMITS; }
interface                       { MVCHARNO(yytext); return T_KW_INTERFACE; }
public                          { MVCHARNO(yytext); return T_KW_PUBLIC; }
typedef                         { MVCHARNO(yytext); return T_KW_TYPEDEF; }
attribute                       { MVCHARNO(yytext); return T_KW_ATTRIBUTE; }
enum                            { MVCHARNO(yytext); return T_KW_ENUM; }
local                           { MVCHARNO(yytext); return T_KW_LOCAL; }
publishes                       { MVCHARNO(yytext); return T_KW_PUBLISHES; }
typeid                          { MVCHARNO(yytext); return T_KW_TYPEID; }
boolean                         { MVCHARNO(yytext); return T_KW_BOOLEAN; }
eventtype                       { MVCHARNO(yytext); return T_KW_EVENTTYPE; }
long                            { MVCHARNO(yytext); return T_KW_LONG; }
raises                          { MVCHARNO(yytext); return T_KW_RAISES; }
typeprefix                      { MVCHARNO(yytext); return T_KW_TYPEPREFIX; }
case                            { MVCHARNO(yytext); return T_KW_CASE; }
factory                         { MVCHARNO(yytext); return T_KW_FACTORY; }
module                          { MVCHARNO(yytext); return T_KW_MODULE; }
readonly                        { MVCHARNO(yytext); return T_KW_READONLY; }
unsigned                        { MVCHARNO(yytext); return T_KW_UNSIGNED; }
char                            { MVCHARNO(yytext); return T_KW_CHAR; }
FALSE                           { MVCHARNO(yytext); return T_KW_FALSE; }
multiple                        { MVCHARNO(yytext); return T_KW_MULTIPLE; }
setraises                       { MVCHARNO(yytext); return T_KW_SETRAISES; }
union                           { MVCHARNO(yytext); return T_KW_UNION; }
component                       { MVCHARNO(yytext); return T_KW_COMPONENT; }
finder                          { MVCHARNO(yytext); return T_KW_FINDER; }
native                          { MVCHARNO(yytext); return T_KW_NATIVE; }
sequence                        { MVCHARNO(yytext); return T_KW_SEQUENCE; }
uses                            { MVCHARNO(yytext); return T_KW_USES; }
const                           { MVCHARNO(yytext); return T_KW_CONST; }
fixed                           { MVCHARNO(yytext); return T_KW_FIXED; }
Object                          { MVCHARNO(yytext); return T_KW_OBJECT; }
short                           { MVCHARNO(yytext); return T_KW_SHORT; }
ValueBase                       { MVCHARNO(yytext); return T_KW_VALUEBASE; }
consumes                        { MVCHARNO(yytext); return T_KW_CONSUMES; }
float                           { MVCHARNO(yytext); return T_KW_FLOAT; }
octet                           { MVCHARNO(yytext); return T_KW_OCTET; }
string                          { MVCHARNO(yytext); return T_KW_STRING; }
valuetype                       { MVCHARNO(yytext); return T_KW_VALUETYPE; }
context                         { MVCHARNO(yytext); return T_KW_CONTEXT; }
getraises                       { MVCHARNO(yytext); return T_KW_GETRAISES; }
oneway                          { MVCHARNO(yytext); return T_KW_ONEWAY; }
struct                          { MVCHARNO(yytext); return T_KW_STRUCT; }
void                            { MVCHARNO(yytext); return T_KW_VOID; }
custom                          { MVCHARNO(yytext); return T_KW_CUSTOM; }
home                            { MVCHARNO(yytext); return T_KW_HOME; }
out                             { MVCHARNO(yytext); return T_KW_OUT; }
supports                        { MVCHARNO(yytext); return T_KW_SUPPORTS; }
wchar                           { MVCHARNO(yytext); return T_KW_WCHAR; }
default                         { MVCHARNO(yytext); return T_KW_DEFAULT; }
import                          { MVCHARNO(yytext); return T_KW_IMPORT; }
primarykey                      { MVCHARNO(yytext); return T_KW_PRIMARYKEY; }
switch                          { MVCHARNO(yytext); return T_KW_SWITCH; }
wstring                         { MVCHARNO(yytext); return T_KW_WSTRING; }
double                          { MVCHARNO(yytext); return T_KW_DOUBLE; }
in                              { MVCHARNO(yytext); return T_KW_IN; }
private                         { MVCHARNO(yytext); return T_KW_PRIVATE; }
TRUE                            { MVCHARNO(yytext); return T_KW_TRUE; }
	/* New in X-types */
map                             { MVCHARNO(yytext); return T_KW_MAP; }

{Identifier}                    { MVCHARNO(yytext); idllval.name=strdup(yytext); return T_IDENTIFIER; }
{IntegerLiteral}                { MVCHARNO(yytext); idllval.literal.kind = INTEGER_KIND; idllval.literal.string=strdup(yytext); return T_L_INTEGER; }
{CharacterLiteral}              { MVCHARNO(yytext); idllval.literal.kind = CHAR_KIND; idllval.literal.string=strdup(yytext);return T_L_CHAR; }
{FloatingPointLiteral}          { MVCHARNO(yytext); idllval.literal.kind = FLOAT_KIND; idllval.literal.string=strdup(yytext);return T_L_FLOAT; }
{StringLiteral}                 { MVCHARNO(yytext); idllval.literal.kind = STRING_KIND; idllval.literal.string=strdup(yytext);return T_L_STRING; }
        /*{FixedPointLiteral}    { MVCHARNO(yytext); printf("String:     %s\n", yytext); } */

        /* Catch all remaining */
.                               { printf("Lexer: unknown character \"%s\" at line %d, offset %d\n", yytext, lineno, charno); exit(0);}

%%

int idlwrap() 
{ 
        return 1; 
}

void idlerror (char *s) {
        fprintf (stderr, "At line %d: %s\n", lineno, s);
        exit(EXIT_FAILURE);
}

