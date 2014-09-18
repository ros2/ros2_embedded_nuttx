-- SQL grammar subset as used in:
--	The filter_expression in the ContentFilteredTopic
--	The topic_expression in the MultiTopic
--	The query_expression in the QueryReadCondition
--
-- The expressions use a subset of SQL, extended with the possibility to use
-- program variables in the SQL expression.
--
-- Special notation:
--	(element // ',')
-- effectively means:
--	element { ',' element }
--
-- The syntax and meaning of the tokens is defined as follows:
--	FIELDNAME    -	References a field in the data-structure.  The '.' is
--			used to navigate in substructures (unlimited nesting).
--	TOPICNAME    -	Identifier of a topic name (only aphanumeric chars).
--	INTEGERVALUE -	Decimal, optionally preceeded by a sign, or hexadecimal.
--	CHARVALUE    -	A single character between single quotes.
--	FLOATVALUE   -	Floating point number.
--	STRING       -	Any series of characters encapsulated in single quotes
--			except a newline or right quote. Strings start with a
--			left or right quote but ends with a right quote.
--	ENUMERATEDVALUE	References a value declared in an enumeration.
--	PARAMETER    -	Of the form %n where n is in [0..99].

Expression 	::=	FilterExpression
		|	TopicExpression
		|	QueryExpression
		.
FilterExpression::= Condition
		.
TopicExpression	::= SelectFrom [Where] ';'
		.
QueryExpression	::= [Condition]['ORDER BY' FIELDNAME {',' FIELDNAME}]
		.
SelectFrom	::=	'SELECT' Aggregation 'FROM' Selection
		.
Aggregation	::=	'*'
		|	SubjectFieldSpec {',' SubjectFieldSpec}
		.
SubjectFieldSpec::=	FIELDNAME
		|	FIELDNAME 'AS' FIELDNAME
		|	FIELDNAME FIELDNAME
		.
Selection	::=	TOPICNAME
		|	TOPICNAME NaturalJoin JoinItem
		.
JoinItem	::=	TOPICNAME
		|	TOPICNAME NaturalJoin JoinItem
		|	'(' TOPICNAME NaturalJoin JoinItem ')'
		.
NaturalJoin	::=	'INNER NATURAL JOIN'
		|	'NATURAL JOIN'
		|	'NATURAL INNER JOIN'
		.
Where		::=	'WHERE' Condition
		.
Condition	::=	Predicate
		|	Condition 'AND' Condition
		|	Condition 'OR' Condition
		|	'NOT' Condition
		|	'(' Condition ')'
		.
Predicate	::=	CompPredicate
		|	BetweenPredicate
		.
CompPredicate	::=	FIELDNAME RelOp Parameter
		|	Parameter RelOp FIELDNAME
		|	FIELDNAME RelOp FIELDNAME
		.
BetweenPredicate::=	FIELDNAME 'BETWEEN' Range
		|	FIELDNAME 'NOT BETWEEN' Range
		.
RelOp		::=	'=' | '>' | '>=' | '<' | '<=' | '<>' | 'LIKE'
		.
Range		::=	Parameter 'AND' Parameter
		.
Parameter	::=	INTEGERVALUE
		|	CHARVALUE
		|	FLOATVALUE
		|	STRING
		|	ENUMERATEDVALUE
		|	PARAMETER
		.


-- In order to take into account proper operator precedence handling,
-- the condition can be rewritten as:

Condition	::=	Term { 'OR' Term }
		.
Term		::=	Factor { 'AND' Factor }
		.
Factor		::=	Predicate
		|	'NOT' Factor
		|	'(' Condition ')'
		.
