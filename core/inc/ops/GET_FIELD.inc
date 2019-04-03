TVal rec = regs.sp[-2];
TVal key = regs.sp[-1];
if( !tvIsObjType( rec, OBJ_REC ) ) {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Attempt to get field from non-Rec type %t",
        rec
    );
}
regs.sp--;
regs.sp[-1] = recGet( state, tvGetObj( rec ), key );
