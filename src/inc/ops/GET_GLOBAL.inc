TVal* val = envGetGlobalByLoc( state, opr );
tenAssert( val );

Upvalue* upv = tvGetObj( *val );
if( tvIsObj( *val ) && datGetTag( (void*)upv ) == OBJ_UPV )
    *(regs.sp++) = upv->val;
else
    *(regs.sp++) = *val;
