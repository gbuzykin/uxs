%start string

dig       [0-9]
hdig      [0-9a-fA-F]
letter    [a-zA-Z]
dec       {dig}+
real      (({dig}+(\.{dig}*)?)|(\.{dig}+))((e|E)(\+?|\-?){dig}+)?
ws        [ \f\r\t\v]

%%

# String literal

escape_unicode  <string> \\u{hdig}{4}
escape_a        <string> \\a
escape_b        <string> \\b
escape_f        <string> \\f
escape_r        <string> \\r
escape_n        <string> \\n
escape_t        <string> \\t
escape_v        <string> \\v
escape_other    <string> \\.

string_seq      <string> [^"\\\n]+
string_nl       <string> \n
string_close    <string> \"

# Other literals
null_literal     "null"
true_literal     "true"
false_literal    "false"
dec_literal      ('+'|'-')?{dec}
real_literal     ('+'|'-')?{real}

# Other
comment          "//"
whitespace       {ws}+
nl               \n
string           \"
other_char       .

%%
