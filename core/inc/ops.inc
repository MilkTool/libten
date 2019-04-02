// The following VM operations are wrapped in a call to
// an OP() macro, which is expected to be defined before
// including the file, this should wrap and filter the
// fields given into a the format needed at the point of
// inclusion.
//
// The fields are given in the following order:
//
// Name:
// The operation name.
//
// Stack Effect:
// This is given as a linear equation in the form of a macro SE(),
// which should be defined before inclusion of the file.  The SE()
// macro is given two fields, a multiplier and an offset.  For
// example the equation `SE( 2, -1 )` would add `2*N - 1` to the
// stack size, where `N` is the instruction's operand value.
// Note that these equations give the minimum stack effect to allow
// us to figure out at compile time how many temporaries might be
// needed by a function; but they can overstate the number of slots
// needed.

OP( DEF_ONE, SE( 0, -1 ) )
OP( DEF_TUP, SE( -2, -1 ) )
OP( DEF_VTUP, SE( -2, -2 ) )
OP( DEF_REC, SE( -1, 0 ) )
OP( DEF_VREC, SE( -1, -1 ) )
OP( DEF_SIG, SE( -2, -1 ) )
OP( DEF_VSIG, SE( -2, -2 ) )
OP( SET_ONE, SE( 0, -1 ) )
OP( SET_TUP, SE( -2, -1 ) )
OP( SET_VTUP, SE( -2, -1 ) )
OP( SET_REC, SE( -1, 0 ) )
OP( SET_VREC, SE( -1, 0 ) )

OP( REC_DEF_ONE, SE( 0, -2 ) )
OP( REC_DEF_TUP, SE( -2, -1 ) )
OP( REC_DEF_VTUP, SE( -2, -2 ) )
OP( REC_DEF_REC, SE( -2, -1 ) )
OP( REC_DEF_VREC, SE( -2, -2 ) )
OP( REC_SET_ONE, SE( 0, -2 ) )
OP( REC_SET_TUP, SE( -2, -1 ) )
OP( REC_SET_VTUP, SE( -2, -2 ) )
OP( REC_SET_REC, SE( -2, -1 ) )
OP( REC_SET_VREC, SE( -2, -2 ) )

OP( GET_CONST, SE( 0, 1 ) )
OP( GET_CONST0, SE( 0, 1 ) )
OP( GET_CONST1, SE( 0, 1 ) )
OP( GET_CONST2, SE( 0, 1 ) )
OP( GET_CONST3, SE( 0, 1 ) )
OP( GET_CONST4, SE( 0, 1 ) )
OP( GET_CONST5, SE( 0, 1 ) )
OP( GET_CONST6, SE( 0, 1 ) )
OP( GET_CONST7, SE( 0, 1 ) )

OP( GET_UPVAL, SE( 0, 1 ) )
OP( GET_UPVAL0, SE( 0, 1 ) )
OP( GET_UPVAL1, SE( 0, 1 ) )
OP( GET_UPVAL2, SE( 0, 1 ) )
OP( GET_UPVAL3, SE( 0, 1 ) )
OP( GET_UPVAL4, SE( 0, 1 ) )
OP( GET_UPVAL5, SE( 0, 1 ) )
OP( GET_UPVAL6, SE( 0, 1 ) )
OP( GET_UPVAL7, SE( 0, 1 ) )

OP( GET_LOCAL, SE( 0, 1 ) )
OP( GET_LOCAL0, SE( 0, 1 ) )
OP( GET_LOCAL1, SE( 0, 1 ) )
OP( GET_LOCAL2, SE( 0, 1 ) )
OP( GET_LOCAL3, SE( 0, 1 ) )
OP( GET_LOCAL4, SE( 0, 1 ) )
OP( GET_LOCAL5, SE( 0, 1 ) )
OP( GET_LOCAL6, SE( 0, 1 ) )
OP( GET_LOCAL7, SE( 0, 1 ) )

OP( GET_CLOSED, SE( 0, 1 ) )
OP( GET_CLOSED0, SE( 0, 1 ) )
OP( GET_CLOSED1, SE( 0, 1 ) )
OP( GET_CLOSED2, SE( 0, 1 ) )
OP( GET_CLOSED3, SE( 0, 1 ) )
OP( GET_CLOSED4, SE( 0, 1 ) )
OP( GET_CLOSED5, SE( 0, 1 ) )
OP( GET_CLOSED6, SE( 0, 1 ) )
OP( GET_CLOSED7, SE( 0, 1 ) )

OP( GET_GLOBAL, SE( 0, 1 ) )

OP( GET_FIELD, SE( 0, -1 ) )

OP( REF_UPVAL, SE( 0, 1 ) )
OP( REF_LOCAL, SE( 0, 1 ) )
OP( REF_CLOSED, SE( 0, 1 ) )
OP( REF_GLOBAL, SE( 0, 1 ) )

OP( LOAD_NIL, SE( 0, 1 ) )
OP( LOAD_UDF, SE( 0, 1 ) )
OP( LOAD_LOG, SE( 0, 1 ) )
OP( LOAD_INT, SE( 0, 1 ) )

OP( MAKE_TUP, SE( 0, 1 ) )
OP( MAKE_VTUP, SE( 0, TUP_MAX ) )
OP( MAKE_CLS, SE( 0, 1 ) )
OP( MAKE_REC, SE( -2, 0 ) )
OP( MAKE_VREC, SE( -2, -1 ) )

OP( POP, SE( -1, -1 ) )

OP( NEG, SE( 0, 0 ) )
OP( NOT, SE( 0, 0 ) )
OP( FIX, SE( 0, 0 ) )

OP( POW, SE( 0, -1 ) )
OP( MUL, SE( 0, -1 ) )
OP( DIV, SE( 0, -1 ) )
OP( MOD, SE( 0, -1 ) )
OP( ADD, SE( 0, -1 ) )
OP( SUB, SE( 0, -1 ) )
OP( LSL, SE( 0, -1 ) )
OP( LSR, SE( 0, -1 ) )
OP( AND, SE( 0, -1 ) )
OP( XOR, SE( 0, -1 ) )
OP( OR, SE( 0, -1 ) )
OP( IMT, SE( 0, -1 ) )
OP( ILT, SE( 0, -1 ) )
OP( IME, SE( 0, -1 ) )
OP( ILE, SE( 0, -1 ) )
OP( IET, SE( 0, -1 ) )
OP( NET, SE( 0, -1 ) )
OP( IETU, SE( 0, -1 ) )
OP( AND_JUMP, SE( 0, 0 ) )
OP( OR_JUMP, SE( 0, 0 ) )
OP( UDF_JUMP, SE( 0, 0 ) )
OP( ALT_JUMP, SE( 0, -1 ) )
OP( JUMP, SE( 0, 0 ) )

OP( CALL, SE( 0, 0 ) )
OP( RETURN, SE( 0, 0 ) )
