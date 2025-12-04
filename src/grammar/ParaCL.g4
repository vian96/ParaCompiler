grammar ParaCL;

program: statement+;

statement
    : varDecl ';'
    | assignment ';'
    | expr ';'
    | output ';'
    ;

varDecl: ID typeSpec? '=' expr;
assignment: ID '=' expr;
output: 'output' '(' '0' ',' expr ')'; 

expr
    : input        # InputExpr
    | INT          # IntExpr
    | ID           # IdExpr
    ;

input: 'input' '(' '0' ')';
typeSpec: ':' 'int';

INT: '-'? [0-9]+;
ID: [a-zA-Z_][a-zA-Z0-9_]*;
WS: [ \t\r\n]+ -> skip;
LC  : '//' ~[\r\n]* -> skip;
