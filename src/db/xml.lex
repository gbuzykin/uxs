%start value

dig       [0-9]
hdig      [0-9a-fA-F]
letter    [a-zA-Z\x80-\xff]
dec       {dig}+
name      (:|_|{letter})(:|_|-|\.|{letter}|{dig})*
ws        [ \t\r]+

%%

null          <value> {ws}?(n|N)(u|U)(l|L)(l|L){ws}?
true          <value> {ws}?(t|T)(r|R)(u|U)(e|E){ws}?
false         <value> {ws}?(f|F)(a|A)(l|L)(s|S)(e|E){ws}?
decimal       <value> {ws}?\+?{dec}{ws}?
neg_decimal   <value> {ws}?-{dec}{ws}?
real          <value> {ws}?(-|\+?)(({dig}+(\.{dig}*)?)|(\.{dig}+))((e|E)(\+?|\-?){dig}+)?{ws}?
ws_with_nl    <value> {ws}?(\n{ws}?)+
other_value   <value> (.|\n)+

amp          &amp;
lt           &lt;
gt           &gt;
apos         &apos;
quot         &quot;
entity       &{name};
dcode        &#{dig}+;
hcode        &#(x|X){hdig}+;
ent_invalid  &

name                {name}
start_element_open  <{name}
end_element_open    <\/{name}
end_element_close   \/>
pi_open             <\?{name}
pi_close            \?>
comment             <\!--

%%
