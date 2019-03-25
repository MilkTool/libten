TVal* fv = regs.sp - opr - 1;
tenAssert( tvIsObj( *fv ) );
tenAssert( datGetTag( tvGetObj( *fv ) ) == OBJ_FUN );

Function* fun = tvGetObj( *fv );
tenAssert( fun->type == FUN_VIR );

VirFun* vir = &fun->u.vir;
tenAssert( opr == vir->nUpvals );

Closure* cls = clsNew( state, fun, NULL );
*fv = tvObj( cls );

TVal* upvs = fv + 1;
for( uint i = 0 ; i < opr ; i++ )
    cls->dat.upvals[i] = upvNew( state, upvs[i] );

regs.sp -= opr;
