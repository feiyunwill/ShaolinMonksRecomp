namespace openshao::aspect {
struct TutorialImage {uint32_t sprite=0;TutorialSprite role=TutorialSprite::None;};
inline thread_local std::array<TutorialImage,3> tutorial_images{};
struct TutorialPrompt {bool valid=false;uint32_t x=0,y=0,characters=0;};
inline thread_local TutorialPrompt tutorial_prompt{};
inline thread_local std::array<TutorialPrompt,4> artwork_footer{};
inline thread_local std::array<bool,512> flash_polygons{};
inline thread_local uint32_t animation_scene=0;
bool ArtworkViewerActive() {
    const uint32_t scene=kernel::R32(0x59847C);
    return scene && kernel::R32(0x5985E4)<2 && kernel::R32(scene+0x2E4)==16;
}
uint32_t AnimationTextureKey(uint32_t scene,uint32_t texture) {
    if(!scene || texture==0xFFFFFFFF)return 0;
    const uint32_t library=kernel::R32(scene+0x2E8);
    if(!library)return 0;
    const uint32_t count=kernel::R32(library+8),entries=kernel::R32(library+12);
    if(!entries || count>4096)return 0;
    for(uint32_t i=4;i<count;++i) {
        const uint32_t entry=entries+i*16;
        if(kernel::R32(entry+8)==texture)return kernel::R32(entry+12);
    }
    return 0;
}
bool IsTransitionScenery(uint32_t scene,uint32_t texture) {
    // Transition scenery uses control timelines, including their companion layers.
    switch(AnimationTextureKey(scene,texture)) {
        case 0x6280E35F:case 0xC46465E6:case 0x9587CD64:case 0x14BA29FA:return true;
        case 0xAB8EFAE9:case 0xFEE6AC1F: // Main menu to Options.
        case 0xB3940E03:case 0x137124EA:case 0x2BD63191: // Main menu to Kontent.
        case 0x25055CA7:case 0xA3DF797D:return true; // Kontent category transitions.
    default:return false;
    }
}
bool LayoutGalleryBackdrop(uint32_t first,uint32_t end) {
    const auto scale=CanvasScale(Current());
    if(scale.x==1 && scale.y==1)return false;
    if(end!=first+2 || first>506)return false;
    // All eight frames are baked into one wall bitmap; only the outer 64 pixels are decorative.
    std::array<uint8_t,0x6C> original{};
    std::memcpy(original.data(),kernel::HostPtr(0x63F5E4+first*0x6C),original.size());
    auto value=[&](unsigned offset){float v;std::memcpy(&v,original.data()+offset,4);return v;};
    const float left=value(0x10),top=value(0x14),right=value(0x30),bottom=value(0x34);
    if(!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) || !std::isfinite(bottom) ||
       right-left<128 || bottom-top<128 || value(0x20)!=left || value(0x24)!=bottom)return false;
    const uint32_t second=0x63F5E4+(first+1)*0x6C;
    auto native_float=[](uint32_t a){return std::bit_cast<float>(kernel::R32(a));};
    if(native_float(second+0x10)!=left || native_float(second+0x14)!=top ||
       native_float(second+0x20)!=right || native_float(second+0x24)!=top ||
       native_float(second+0x30)!=right || native_float(second+0x34)!=bottom ||
       value(0x40)!=value(0x44) || value(0x50)!=value(0x54))return false;
    // Mirror only decorative edges at their original density. Preflight the queue to avoid partial walls.
    struct Tile {float x0,y0,x1,y1,u0,v0,u1,v1;};
    std::array<Tile,256> tiles{};
    unsigned count=1;
    const float x0=(left+.5f-320)*scale.x+319.5f,y0=(top+.5f-240)*scale.y+239.5f;
    const float x1=(right+.5f-320)*scale.x+319.5f,y1=(bottom+.5f-240)*scale.y+239.5f;
    tiles[0]={x0,y0,x1,y1,left,top,right,bottom};
    const bool horizontal=scale.x<1;
    const float step=64*(horizontal?scale.x:scale.y),limit=horizontal?639.5f:479.5f;
    if(!std::isfinite(step) || step<=0)return false;
    for(unsigned side=0;side<2;++side) {
        float edge=horizontal?(side?x1:x0):(side?y1:y0);
        for(unsigned repeat=0;side?edge<limit:edge>-.5f;++repeat) {
            if(count>=tiles.size() || first+(count+1)*2>512)return false;
            const float outer=side?std::min(limit,edge+step):std::max(-.5f,edge-step);
            if(outer==edge)return false;
            const float extent=std::abs(outer-edge)/(horizontal?scale.x:scale.y);
            const float source_edge=horizontal?(side?right:left):(side?bottom:top);
            const float direction=side?-1.f:1.f;
            const float inner_source=source_edge+direction*(repeat%2?64:0);
            const float outer_source=inner_source+direction*(repeat%2?-extent:extent);
            Tile tile=tiles[0];
            if(horizontal) {
                tile.x0=side?edge:outer;tile.x1=side?outer:edge;
                tile.u0=side?inner_source:outer_source;tile.u1=side?outer_source:inner_source;
            } else {
                tile.y0=side?edge:outer;tile.y1=side?outer:edge;
                tile.v0=side?inner_source:outer_source;tile.v1=side?outer_source:inner_source;
            }
            tiles[count++]=tile;edge=outer;
        }
    }
    const unsigned corners[2][3]={{0,2,3},{0,1,3}};
    for(unsigned tile=0;tile<count;++tile)for(unsigned triangle=0;triangle<2;++triangle) {
        const uint32_t index=first+tile*2+triangle,record=0x63F5E4+index*0x6C;
        std::memcpy(kernel::HostPtr(record),original.data(),original.size());
        const auto& t=tiles[tile];
        for(unsigned v=0;v<3;++v) {
            const unsigned corner=corners[triangle][v];
            const float x=corner%2?t.x1:t.x0,y=corner/2?t.y1:t.y0;
            const float u=corner%2?t.u1:t.u0,w=corner/2?t.v1:t.v0;
            kernel::W32(record+0x10+v*16,std::bit_cast<uint32_t>(x));
            kernel::W32(record+0x14+v*16,std::bit_cast<uint32_t>(y));
            const float s=value(0x40)+(value(0x48)-value(0x40))*(u-left)/(right-left);
            const float r=value(0x4C)+(value(0x50)-value(0x4C))*(w-top)/(bottom-top);
            kernel::W32(record+0x40+v*4,std::bit_cast<uint32_t>(s));
            kernel::W32(record+0x4C+v*4,std::bit_cast<uint32_t>(r));
        }
        flash_polygons[index]=false;
    }
    kernel::W32(0x63F5E0,first+count*2);
    return true;
}
void ExpandModalShade(uint32_t first,uint32_t end) {
    const auto scale=CanvasScale(Current());
    if(first>end || end>512 || (scale.x==1 && scale.y==1))return;
    std::array<bool,512> shade{};
    float left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY;
    for(uint32_t i=first;i<end;++i) {
        const uint32_t record=0x63F5E4+i*0x6C,color=kernel::R32(record+0x58);
        if(kernel::R32(record+0x68) || !(color>>24) || (color&0xFFFFFF) ||
           color!=kernel::R32(record+0x5C) || color!=kernel::R32(record+0x60))continue;
        bool finite=true;
        for(unsigned v=0;v<3;++v)for(unsigned axis=0;axis<2;++axis)
            finite&=std::isfinite(std::bit_cast<float>(kernel::R32(record+0x10+v*16+axis*4)));
        if(!finite)continue;
        shade[i]=true;
        for(unsigned v=0;v<3;++v) {
            const float x=std::bit_cast<float>(kernel::R32(record+0x10+v*16));
            const float y=std::bit_cast<float>(kernel::R32(record+0x14+v*16));
            left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
        }
    }
    // Expand the outer veil silhouette; keep its opening aligned with the separate dialog panel.
    if(left>320*(1-scale.x)-.5f+2*scale.x || right<320*(1+scale.x)-.5f-2*scale.x ||
       top>240*(1-scale.y)-.5f+2*scale.y || bottom<240*(1+scale.y)-.5f-2*scale.y)return;
    for(uint32_t i=first;i<end;++i)if(shade[i]) {
        const uint32_t record=0x63F5E4+i*0x6C;
        for(unsigned v=0;v<3;++v) {
            const auto address=record+0x10+v*16;
            const float x=std::bit_cast<float>(kernel::R32(address)),y=std::bit_cast<float>(kernel::R32(address+4));
            // Flash's separate bands can differ by subpixel amounts along one
            // authored edge. Group points that round to the same source pixel.
            if(std::round((x-left)/scale.x)==0)kernel::W32(address,std::bit_cast<uint32_t>(-.5f));
            else if(std::round((x-right)/scale.x)==0)kernel::W32(address,std::bit_cast<uint32_t>(639.5f));
            if(std::round((y-top)/scale.y)==0)kernel::W32(address+4,std::bit_cast<uint32_t>(-.5f));
            else if(std::round((y-bottom)/scale.y)==0)kernel::W32(address+4,std::bit_cast<uint32_t>(479.5f));
        }
        flash_polygons[i]=false;
    }
}
void UpdateCamera() {
    // CEngineRenderer::m_pWorldCamera. Keep the original viewport and both FOV fields.
    const uint32_t world=kernel::R32(0x639224);
    if(!world)return;
    const uint32_t camera=kernel::R32(world+0x308);
    if(camera)kernel::W32(camera+0x20,std::bit_cast<uint32_t>(CameraScale(Current())));
}
}
X86_EXTERN_FUNC(__imp__sub_00016866);
X86_FUNC(sub_00016866) {
    // The message manager queues the band and both portrait layers before rendering them.
    const bool message=ctx.ecx==0x4E6E20 && kernel::R32(ctx.esp+4)==0xE;
    const uint32_t image=kernel::R32(ctx.esp+8);
    __imp__sub_00016866(ctx,base);
    if(!message || !ctx.eax || (image!=0xA9 && image!=0x9D && image!=0x9E))return;
    for(auto& item:openshao::aspect::tutorial_images)if(item.sprite==ctx.eax)item={};
    const unsigned index=image==0xA9?0:image==0x9D?1:2;
    openshao::aspect::tutorial_images[index]={ctx.eax,image==0xA9?openshao::aspect::TutorialSprite::Panel:openshao::aspect::TutorialSprite::Portrait};
    if(image==0x9D) {
        const uint32_t definition=kernel::R32(ctx.eax+0x1D8);
        const float x=std::bit_cast<float>(kernel::R32(ctx.eax+0xC0));
        const float width=definition?float(kernel::R32(definition+4)&0xFFFF):0;
        // The original portrait extends beyond x=640. Move both atlas layers and the confirmation glyph together.
        openshao::aspect::tutorial_portrait_inset=width && std::isfinite(x)?std::max(0.f,x+width*.5f-639.5f):0;
    }
}
X86_EXTERN_FUNC(__imp__sub_000165C3);
X86_FUNC(sub_000165C3) {
    const bool message=ctx.ecx==0x4E6E20;
    __imp__sub_000165C3(ctx,base);
    if(message){openshao::aspect::tutorial_images.fill({});openshao::aspect::tutorial_prompt={};openshao::aspect::tutorial_portrait_inset=0;}
}
X86_EXTERN_FUNC(__imp__sub_0011B290);
X86_EXTERN_FUNC(__imp__sub_00152AE0);
X86_FUNC(sub_00152AE0) {
    if(kernel::R32(ctx.esp)==0x48F0C) {
        openshao::menu::MainControllerNotice(ctx,kernel::R32(ctx.esp+20),kernel::R32(ctx.esp+12),
            std::bit_cast<float>(kernel::R32(ctx.esp+24)));
        kernel::Return(ctx,0,0);return;
    }
    // Move the native navigation and counter into one footer; preserve localization and input glyphs.
    const uint32_t caller=kernel::R32(ctx.esp),y_address=ctx.esp+8;
    const uint32_t original_y=kernel::R32(y_address);
    if(openshao::aspect::ArtworkViewerActive()) {
        unsigned index=4;
        if(caller==0x4C822)index=0;
        else if(caller==0x4C857)index=1;
        else if(caller==0x4C942)index=2;
        else if(caller==0x4C973)index=3;
        if(index<4) {
            const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
            const auto y=uint32_t(std::lround((index%2?446.f:448.f)+240*(1/scale.y-1)));
            kernel::W32(y_address,y);
            const uint32_t text=kernel::R32(ctx.esp+20);
            openshao::aspect::artwork_footer[index]={text!=0,
                std::bit_cast<uint32_t>(float(int32_t(kernel::R32(ctx.esp+4)))),
                std::bit_cast<uint32_t>(float(y)),text?kernel::R32(text)&0xFFFF:0};
        }
    }
    // The confirmation glyph is queued separately; track its coordinates without shifting the message body.
    if(kernel::R32(ctx.esp)==0x86714) {
        const uint32_t type=kernel::R32(0x4E6E20+0x6D4),text=kernel::R32(ctx.esp+20);
        if(text && (type==6 || type==8 || type==9)) {
            const float x=float(int32_t(kernel::R32(ctx.esp+4))),y=float(int32_t(kernel::R32(ctx.esp+8)));
            openshao::aspect::tutorial_prompt={true,std::bit_cast<uint32_t>(x),std::bit_cast<uint32_t>(y),kernel::R32(text)&0xFFFF};
        }
    }
    __imp__sub_00152AE0(ctx,base);
    kernel::W32(y_address,original_y);
}
X86_EXTERN_FUNC(__imp__sub_001EF210);
X86_FUNC(sub_001EF210) {
    const uint32_t command=kernel::R32(ctx.esp+4),text=command?kernel::R32(command+0x34):0;
    if(text && openshao::aspect::ArtworkViewerActive())for(const auto& footer:openshao::aspect::artwork_footer) {
        if(!footer.valid || kernel::R32(command)!=footer.x || kernel::R32(command+4)!=footer.y ||
           (kernel::R32(text)&0xFFFF)!=footer.characters)continue;
        ++openshao::aspect::viewport_text_depth;
        __imp__sub_001EF210(ctx,base);
        --openshao::aspect::viewport_text_depth;
        return;
    }
    const auto prompt=openshao::aspect::tutorial_prompt;
    if(!prompt.valid || !text || kernel::R32(command)!=prompt.x || kernel::R32(command+4)!=prompt.y ||
       (kernel::R32(text)&0xFFFF)!=prompt.characters){__imp__sub_001EF210(ctx,base);return;}
    const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
    kernel::W32(command,std::bit_cast<uint32_t>(std::bit_cast<float>(prompt.x)+320.f*(1/scale.x-1)-openshao::aspect::tutorial_portrait_inset));
    ++openshao::aspect::viewport_text_depth;
    __imp__sub_001EF210(ctx,base);
    --openshao::aspect::viewport_text_depth;
    kernel::W32(command,prompt.x);
}
X86_FUNC(sub_0011B290) {
    __imp__sub_0011B290(ctx,base);
    openshao::aspect::UpdateCamera();
}
X86_EXTERN_FUNC(__imp__sub_0015AFA0);
X86_FUNC(sub_0015AFA0) {
    __imp__sub_0015AFA0(ctx,base);
    // Track camera replacement and window resizing on the guest input thread, including movie playback.
    openshao::aspect::UpdateCamera();
}
X86_EXTERN_FUNC(__imp__sub_0011D000);
X86_FUNC(sub_0011D000) {
    const uint32_t entry=kernel::R32(ctx.esp+4);
    const uint32_t sprite=entry?kernel::R32(entry+8):0;
    if(entry && openshao::aspect::Current()>openshao::aspect::Original+.0001f) {
        // Preserve the native cinematic-mask animation; presentation adds bars for wider viewports.
        if(sprite && (sprite==kernel::R32(0x6CE8A0) || sprite==kernel::R32(0x6CE8A4))) {
            kernel::Return(ctx,0,1);return;
        }
    }
    // Match the atlas white-fill tile, not logical dimensions, to distinguish tints from artwork.
    const uint32_t definition=entry?kernel::R32(entry):0;
    const bool solid_fill=definition &&
        kernel::R32(definition+0x2C)==std::bit_cast<uint32_t>(.83203125f) &&
        kernel::R32(definition+0x30)==std::bit_cast<uint32_t>(.55078125f) &&
        kernel::R32(definition+0x44)==std::bit_cast<uint32_t>(.85546875f) &&
        kernel::R32(definition+0x48)==std::bit_cast<uint32_t>(.57421875f);
    auto tutorial=openshao::aspect::TutorialSprite::None;
    if(sprite)for(const auto& item:openshao::aspect::tutorial_images)if(item.sprite==sprite)tutorial=item.role;
    // Only fit the separate full-image owner while the concept or photo viewer is active.
    const bool artwork=sprite && sprite==kernel::R32(0x5984B0) && openshao::aspect::ArtworkViewerActive();
    openshao::aspect::SpriteScope sprite_scope(solid_fill,tutorial,artwork);
    __imp__sub_0011D000(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_00100B44);
X86_FUNC(sub_00100B44) {
    const uint32_t caller=kernel::R32(ctx.esp),matrix=kernel::R32(ctx.esp+4);
    __imp__sub_00100B44(ctx,base);
    // These calls build centered screen-space projections; other matrix callers keep the native path.
    if(matrix && (caller==0x11C9D3 || caller==0x12F8E1)) {
        const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
        for(const auto [offset,factor]:{std::pair{0u,scale.x},std::pair{20u,scale.y}}) {
            if(factor!=1)kernel::W32(matrix+offset,std::bit_cast<uint32_t>(std::bit_cast<float>(kernel::R32(matrix+offset))*factor));
        }
    }
}
X86_EXTERN_FUNC(__imp__sub_0011D140);
X86_FUNC(sub_0011D140) {
    openshao::aspect::CanvasScope canvas;
    __imp__sub_0011D140(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_0012F930);
X86_FUNC(sub_0012F930) {
    openshao::aspect::CanvasScope canvas;
    __imp__sub_0012F930(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_0013FD70);
X86_FUNC(sub_0013FD70) {
    __imp__sub_0013FD70(ctx,base);
    openshao::aspect::flash_polygons.fill(false);
    openshao::menu::BeginFrame();
}
X86_EXTERN_FUNC(__imp__sub_0013F980);
X86_FUNC(sub_0013F980) {
    const uint32_t first=0x63F5E4,offset=ctx.ecx-first;
    if(ctx.ecx>=first && offset%0x6C==0 && openshao::menu::ViewportArrow(offset/0x6C)) {
        // Header underlines extend to the viewport edge even inside a parent canvas draw.
        ++openshao::aspect::viewport_text_depth;
        __imp__sub_0013F980(ctx,base);
        --openshao::aspect::viewport_text_depth;
        return;
    }
    if(ctx.ecx>=first && offset%0x6C==0 && offset/0x6C<512 && openshao::aspect::flash_polygons[offset/0x6C]) {
        openshao::aspect::CanvasScope canvas;
        __imp__sub_0013F980(ctx,base);
    } else __imp__sub_0013F980(ctx,base);
}
X86_EXTERN_FUNC(__imp__sub_0013FFB0);
X86_FUNC(sub_0013FFB0) {
    // Flash polygons render later, so transform positions when Object2DDraw queues them.
    const uint32_t first=kernel::R32(0x63F5E0);
    const uint32_t menu_key=openshao::aspect::AnimationTextureKey(openshao::aspect::animation_scene,kernel::R32(ctx.esp+12));
    const bool gallery=openshao::aspect::animation_scene==0x58F210 &&
        openshao::aspect::AnimationTextureKey(openshao::aspect::animation_scene,kernel::R32(ctx.esp+12))==0x3BF25039;
    const bool backdrop=(!gallery && openshao::aspect::backdrop_depth!=0) ||
        openshao::aspect::IsTransitionScenery(openshao::aspect::animation_scene,kernel::R32(ctx.esp+12));
    __imp__sub_0013FFB0(ctx,base);
    const uint32_t end=kernel::R32(0x63F5E0);
    if(first>end || end>512)return;
    if(openshao::recomp::Graphic(ctx,openshao::aspect::animation_scene,menu_key,first,end))return;
    if(openshao::menu::ReplaceGraphic(ctx,menu_key,first,end))return;
    if(gallery && openshao::aspect::LayoutGalleryBackdrop(first,end))return;
    openshao::aspect::CanvasScope canvas;
    for(uint32_t i=first;i<end;++i) {
        openshao::aspect::CorrectUiVertices(kernel::HostPtr(0x63F5E4+i*0x6C+0x10),3,16,backdrop);
        openshao::aspect::flash_polygons[i]=!backdrop;
    }
}
X86_EXTERN_FUNC(__imp__sub_001B2C40);
X86_FUNC(sub_001B2C40) {
    // Background instances fill the viewport; titles, selectors and character art use the fitted canvas.
    const bool backdrop=ctx.ecx==0x593A40 || ctx.ecx==0x58E838 || ctx.ecx==0x5912E0 ||
        ctx.ecx==0x58F558 || ctx.ecx==0x593068 || ctx.ecx==0x5929D8 || ctx.ecx==0x591970 ||
        ctx.ecx==0x58E4F0 || ctx.ecx==0x591628 || ctx.ecx==0x58F210 || ctx.ecx==0x5936F8;
    const uint32_t first=kernel::R32(0x63F5E0);
    const bool gallery_arrows=(ctx.ecx==0x58EB80 || ctx.ecx==0x58EEC8) &&
        kernel::R32(0x5984B0) && openshao::aspect::ArtworkViewerActive();
    const uint32_t previous=openshao::aspect::animation_scene;
    openshao::aspect::animation_scene=ctx.ecx;
    if(backdrop)++openshao::aspect::backdrop_depth;
    const bool custom_selector=ctx.ecx==0x592D20 && openshao::recomp::Active();
    const auto depth=kernel::R32(0x6C9308);
    const bool extra_selector=ctx.ecx==0x592D20 && !custom_selector && openshao::recomp::ExtraSelected() &&
        depth<32 && kernel::R32(0x6C9308+depth*4)==0x598430 && kernel::R32(0x598468)>=7 && kernel::R32(0x598468)<9;
    const auto selector_y=ctx.esp+20,previous_y=kernel::R32(selector_y);
    if(custom_selector)kernel::W32(selector_y,openshao::recomp::SelectorY());
    // The retail main selector uses selection modulo seven, including the rows added by the port.
    else if(extra_selector)kernel::W32(selector_y,previous_y+(kernel::R32(0x598468)/7)*7*(kernel::R32(0x4DF4DC)+(kernel::R32(0x5CE864)==1?2:0)));
    openshao::menu::BeginScene();
    __imp__sub_001B2C40(ctx,base);
    if(custom_selector || extra_selector)kernel::W32(selector_y,previous_y);
    const uint32_t before_menu=kernel::R32(0x63F5E0);
    openshao::menu::EndScene(ctx);
    for(uint32_t i=before_menu;i<kernel::R32(0x63F5E0) && i<512;++i)openshao::aspect::flash_polygons[i]=true;
    openshao::aspect::animation_scene=previous;
    // The fitted viewer exposes page arrows hidden by the original bitmap; omit their queued polygons.
    if(gallery_arrows) {
        for(uint32_t i=first;i<kernel::R32(0x63F5E0) && i<512;++i)openshao::aspect::flash_polygons[i]=false;
        kernel::W32(0x63F5E0,first);
    }
    if(backdrop)--openshao::aspect::backdrop_depth;
    else openshao::aspect::ExpandModalShade(first,kernel::R32(0x63F5E0));
}
X86_EXTERN_FUNC(__imp__sub_0012FDE0);
X86_FUNC(sub_0012FDE0) {
    // Expand full-viewport rectangles; fit partial widgets on the same canvas as their sprite borders.
    const uint32_t rectangle=ctx.ecx;
    std::array<uint32_t,4> original{};std::array<float,4> xy{};
    for(unsigned i=0;i<4;++i)xy[i]=std::bit_cast<float>(original[i]=kernel::R32(rectangle+4+i*4));
    const bool viewport=xy[0]<=0 && xy[1]>=640 && xy[2]<=0 && xy[3]>=480;
    if(viewport){__imp__sub_0012FDE0(ctx,base);return;}
    const auto scale=openshao::aspect::CanvasScale(openshao::aspect::Current());
    for(unsigned i=0;i<4;++i) {
        const float center=i<2?320.f:240.f,factor=i<2?scale.x:scale.y;
        kernel::W32(rectangle+4+i*4,std::bit_cast<uint32_t>((xy[i]-center)*factor+center));
    }
    {
        openshao::aspect::CanvasScope canvas;
        __imp__sub_0012FDE0(ctx,base);
    }
    for(unsigned i=0;i<4;++i)kernel::W32(rectangle+4+i*4,original[i]);
}
X86_EXTERN_FUNC(__imp__sub_00141BF0);
X86_FUNC(sub_00141BF0) {
    // The movie system's textured screen quad, four XYZ/RHW/UV vertices.
    const uint32_t quad=kernel::R32(ctx.esp+4);
    const uint32_t width=kernel::R32(ctx.esp+8),height=kernel::R32(ctx.esp+12);
    __imp__sub_00141BF0(ctx,base);
    const float ratio=openshao::aspect::Current();
    if(!quad || !width || !height || std::abs(ratio-openshao::aspect::Original)<.0001f)return;
    const auto rect=openshao::aspect::MovieRect(float(kernel::R32(0x50FB04)),float(kernel::R32(0x50FB08)),ratio,float(width),float(height));
    if(rect.w<=0 || rect.h<=0)return;
    const float xy[4][2]={{rect.x-.5f,rect.y-.5f},{rect.x+rect.w-.5f,rect.y-.5f},
                          {rect.x-.5f,rect.y+rect.h-.5f},{rect.x+rect.w-.5f,rect.y+rect.h-.5f}};
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<2;++j)
        kernel::W32(quad+i*24+j*4,std::bit_cast<uint32_t>(xy[i][j]));
}
