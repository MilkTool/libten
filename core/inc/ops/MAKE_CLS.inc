TVal* fv = regs.sp - opr - 1;
tenAssert( tvIsObj( *fv ) );
tenAssert( datGetTag( tvGetObj( *fv ) ) == OBJ_FUN );

Function* fun = tvGetObj( *fv );
tenAssert( fun->type == FUN_VIR );

VirFun* vir = &fun->u.vir;
tenAssert( opr == vir->nUpvals );

Closure* cls = clsNewVir( state, fun, NULL );
*fv = tvObj( cls );

TVal* refs = fv + 1;
for( uint i = 0 ; i < opr ; i++ ) {
    tenAssert( tvIsRef( refs[i] ) );
    RefT ref = tvGetRef( refs[i] );
    
    refUpv( ref, cls->dat.upvals[i] );
}

regs.sp -= opr;
