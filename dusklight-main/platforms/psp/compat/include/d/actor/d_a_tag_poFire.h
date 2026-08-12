#ifndef DUSK_PSP_COMPAT_D_A_TAG_POFIRE_H
#define DUSK_PSP_COMPAT_D_A_TAG_POFIRE_H

#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_hostIO.h"
#include "m_Do/m_Do_mtx.h"

class daTagPoFire_c : public fopAc_ac_c {
public:
    void setBaseMtx();
    int create();
    int Execute();
    int Draw();
    int Delete();

    void setFireFlag(u8 flag) {
        field_0x569 = flag;
    }

private:
    u8 field_0x568;
    u8 field_0x569;
    u16 field_0x56a;
};

class daTagPoFire_HIO_c : public mDoHIO_entry_c {
public:
    daTagPoFire_HIO_c();
    ~daTagPoFire_HIO_c() override;
    void genMessage(JORMContext*);

    u8 unk_0x4;
};

#endif
