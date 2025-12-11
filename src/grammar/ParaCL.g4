grammar ParaCL;

program: statement+;

statement
    : assignment ';'
    | expr ';'
    | output ';'
    ;

assignment: ID typeSpec ('=' expr)? | ID '=' expr;
output: 'output' '(' INT ',' expr ')';   // TODO: check for 0

expr
    : '(' expr ')' # BracketExpr
    | input        # InputExpr
    | INT          # IntExpr
    | ID           # IdExpr
    | '-' expr # UnaryExpr
    | expr ( '*' | '/') expr # MulExpr
    | expr ( '+' | '-') expr # AddExpr
    | expr ('<=' | '>=' | '<' | '>' | '==' | '!=') expr # CmpExpr
    | expr '&&' expr # AndExpr
    | expr '||' expr # OrExpr
    ;

input: 'input' '(' INT ')';  // TODO: check for 0
typeSpec: ':' 'int';

INT: [0-9]+;  // TODO: are negative numbers required?
ID: [a-zA-Z_][a-zA-Z0-9_]*;
WS: [ \t\r\n]+ -> skip;
LC  : '//' ~[\r\n]* -> skip;
