grammar ParaCL;

program: statement+;

statement
    : assignment ';'
    | expr ';'
    | output ';'
    | ifStatement
    | whileStatement
    | forStatement
    | returnStatement
    ;

block: '{' statement* '}' | statement;

ifStatement: 'if' '(' expr ')' block ('else' block)?;
whileStatement: 'while' '(' expr ')' block;
forStatement: 'for' '(' ID 'in' expr ':' expr (':' expr)? ')' block;
returnStatement: 'return' expr;

assignment: expr typeSpec ('=' expr)? | expr '=' expr;
output: 'output' '(' INT ',' expr ')';

glueEntry: expr (':' ID)?;

expr
    : '(' expr ')' # BracketExpr
    | '{' statement+ '}' # FuncExpr
    | input        # InputExpr
    | INT          # IntExpr
    | ID           # IdExpr
    | 'glue' '(' glueEntry (',' glueEntry)* ')' # GlueExpr
    | expr '.' ID       # DotExpr
    | expr '[' INT ']'  # IndexExpr
    | '-' expr          # UnaryExpr
    | expr ( '*' | '/') expr # MulExpr
    | expr ( '+' | '-') expr # AddExpr
    | expr ('<=' | '>=' | '<' | '>' | '==' | '!=') expr # CmpExpr
    | expr '&&' expr # AndExpr
    | expr '||' expr # OrExpr
    ;

input: 'input' '(' INT ')';
typeSpec: ':' (
    'int' ('(' INT ')')? 
    | '(' ID typeSpec (',' ID typeSpec)* ')'
);

INT: [0-9]+;
ID: [a-zA-Z_][a-zA-Z0-9_]*;
WS: [ \t\r\n]+ -> skip;
LC  : '//' ~[\r\n]* -> skip;
