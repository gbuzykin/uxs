%start string

dig       [0-9]
hdig      [0-9a-fA-F]
letter    [a-zA-Z]
dec       0|[1-9]{dig}*

%%

# String literal
escape_quot     <string> \\\"
escape_rev_sol  <string> \\\\
escape_sol      <string> \\\/
escape_b        <string> \\b
escape_f        <string> \\f
escape_n        <string> \\n
escape_r        <string> \\r
escape_t        <string> \\t
escape_unicode  <string> \\u{hdig}{4}
escape_other    <string> \\.
string_seq      <string> [^"\\\n]+
string_close    <string> [^"\\\n]*\"

# Other literals
null     "null"
true     "true"
false    "false"
decimal  (-|\+)?{dec}
real     (-|\+)?({dec}(\.{dig}+)?((e|E)(\+|-)?{dig}+)?)

# Other
comment          "//"
c_comment        "/*"
whitespace       [ \t\r]+

%%
