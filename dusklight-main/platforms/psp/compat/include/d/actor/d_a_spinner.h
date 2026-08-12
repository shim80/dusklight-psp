#ifndef DUSK_PSP_COMPAT_D_A_SPINNER_H
#define DUSK_PSP_COMPAT_D_A_SPINNER_H

#include "f_op/f_op_actor_mng.h"

// Bounded source-compatible view of the real daSpinner_c contract consumed by
// d_a_obj_swspinner.cpp. This is an input facade, not a port of daSpinner_c.
class daSpinner_c : public fopAc_ac_c {
public:
    enum daSpinner_TAG {
        TAG_NONE,
        TAG_START,
        TAG_2,
        TAG_INTO,
        TAG_INTO_INC_ROT,
        TAG_END,
    };

    void setSpinnerTag(const cXyz& position) {
        if (spinner_tag_ == TAG_NONE) {
            spinner_tag_ = TAG_START;
        }
        tag_position_ = position;
    }
    void offSpinnerTag() {
        if (spinner_tag_ != TAG_NONE) {
            spinner_tag_ = TAG_END;
        }
    }
    bool checkSpinnerTagInto() const {
        return spinner_tag_ == TAG_INTO;
    }
    bool checkSpinnerTagIntoIncRot() const {
        return spinner_tag_ == TAG_INTO_INC_ROT;
    }
    s16 getAngleY() const { return shape_angle.y; }

    void duskPspPublish(s16 angle, daSpinner_TAG tag) {
        shape_angle.y = angle;
        spinner_tag_ = tag;
    }
    daSpinner_TAG duskPspTag() const { return spinner_tag_; }

private:
    cXyz tag_position_ = {};
    daSpinner_TAG spinner_tag_ = TAG_NONE;
};

#endif
