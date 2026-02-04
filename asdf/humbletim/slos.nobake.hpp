#ifndef NOBAKE_H
#define NOBAKE_H

namespace slos {
    extern bool reenableBaking();
    extern bool disableBaking();
    struct BakingDisabler;
}

#endif

#ifdef NOBAKE_IMPLEMENTATION

#include <boost/function.hpp>
extern void doOnIdleRepeating(boost::function<bool()>);

namespace FSCommon { void report_to_nearby_chat(std::string_view message); }

namespace slos {

    struct LLDisabledTexLayerSetBuffer : LLViewerTexLayerSetBuffer {  
        LLDisabledTexLayerSetBuffer(LLTexLayerSet* owner, S32 width, S32 height) : LLViewerTexLayerSetBuffer(owner, width, height) {}  
    };

    bool reenableBaking() {
        if (!isAgentAvatarValid()) return false;
        auto mBakedTextureDatas = via_foaf(gAgentAvatarp, { return mBakedTextureDatas; });
        bool done = true;
        for (int i = 0; i < mBakedTextureDatas.size(); i++) {
            if (auto layerset = mBakedTextureDatas[i].mTexLayerSet) {  
                if (auto comp = via_foaf(layerset, { return mComposite.get(); })) {
                    auto disabled = dynamic_cast<LLDisabledTexLayerSetBuffer*>(comp);  
                    if (disabled) {
                        fprintf(stdout, "blah -- ON tex_layer[%d] cur=%p dis=%p\n", i, comp, disabled);fflush(stdout);
                        FSCommon::report_to_nearby_chat(llformat("blah -- ON tex_layer[%d] cur=%p dis=%p\n", i, comp, disabled));
                        layerset->destroyComposite();
                        done = false;
                    }  
                }
            }
        }
        gAgentAvatarp->invalidateAll();  
        return done;
    }

    bool disableBaking() {
        fprintf(stdout, "blah -- disable bakingings\n");fflush(stdout);
        doOnIdleRepeating([]() -> bool { 
            if (!isAgentAvatarValid()) return false;
            auto mBakedTextureDatas = via_foaf(gAgentAvatarp, { return mBakedTextureDatas; });
            bool done = true;
            for (int i = 0; i < mBakedTextureDatas.size(); i++) {
                if (auto layerset = mBakedTextureDatas[i].mTexLayerSet) {  
                    if (auto comp = via_foaf(layerset, { return mComposite.get(); })) {
                        auto disabled = dynamic_cast<LLDisabledTexLayerSetBuffer*>(comp);  
                        if (!disabled) {
                            fprintf(stdout, "blah -- OFF tex_layer[%d] cur=%p dis=%p\n", i, comp, disabled);fflush(stdout);
                            FSCommon::report_to_nearby_chat(llformat("blah -- OFF tex_layer[%d] cur=%p dis=%p\n", i, comp, disabled));
                            via_foaf(layerset, { mComposite = new LLDisabledTexLayerSetBuffer(this, 512, 512); });
                            done = false;
                        }  
                    }
                }
            }
            return done;
        });
        return false;
    }

    struct BakingDisabler {
        BakingDisabler() {
            gMessageSystem.callWhenReady([](auto) { disableBaking(); });
        }
    };
} // ns

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -DNOBAKE_IMPLEMENTATION"
#endif
