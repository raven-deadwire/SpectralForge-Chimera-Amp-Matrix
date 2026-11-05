#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cstdint>
#include <vector>

namespace spectralforge::cabRoom {
// Room Space surfaces shown on the room scenery (view only; the audio uses
// RoomModel). The scenery photo is fixed, so each surface region is recoloured
// per pixel to the material, keeping the photo's own light pools and shading,
// and the material's pattern (seams, courses, tiles, folds) is drawn on top.
// Regions, measured on room-studio.png as fractions of its height: ceiling
// light strip 0-2 %, walls 2-61 %, skirting ledge 61-64 % (left as
// photographed), floor 64-100 %.
struct SpaceSurfaces {
    bool on{};int floor{-1},walls{-1},ceiling{-1};
    bool operator==(const SpaceSurfaces&) const=default;
};

struct MaterialLook {
    float r,g,b;      // surface colour
    float amount;     // how far the photo is recoloured (0 keeps the photo)
    float contrast;   // how strongly the photo's light pools show through
};
// Indexed like roomMaterials (RoomModel.h). The photographed walls are timber
// slats and the floor is wood, so wood on those surfaces is only lightly tinted.
inline MaterialLook materialLook(int material,bool floor,bool walls) {
    switch(material) {
        case 0: return {.58f,.59f,.57f,.88f,1.4f};          // Concrete
        case 1: return {.56f,.26f,.18f,.85f,1.5f};          // Brick
        case 2: return {.84f,.80f,.72f,.88f,1.2f};          // Plaster
        case 3: return {.86f,.86f,.83f,.88f,1.2f};          // Gypsum board
        case 4: return {.48f,.32f,.19f,floor?.3f:.75f,1.8f};// Wood floor
        case 5: return {.42f,.28f,.17f,walls?.12f:.75f,1.8f};// Wood panel
        case 6: return {.33f,.30f,.28f,.92f,.6f};           // Carpet (matte)
        case 7: return {.45f,.13f,.15f,.9f,.9f};            // Heavy curtain
        case 8: return {.80f,.79f,.75f,.88f,1.1f};          // Acoustic tile
        case 9: return {.45f,.58f,.66f,.65f,2.2f};          // Glass (reflective)
        default: return {0,0,0,0,0};
    }
}

// Recolour `area` of `image` to the material, preserving relative lighting.
inline void recolourSurface(juce::Image& image,juce::Rectangle<int> area,int material,bool floor,bool walls,float fadeFrom=1.f) {
    area=area.getIntersection(image.getBounds());
    const auto look=materialLook(material,floor,walls);
    if(area.isEmpty() || look.amount<=0)return;
    juce::Image::BitmapData pixels(image,area.getX(),area.getY(),area.getWidth(),area.getHeight(),juce::Image::BitmapData::readWrite);
    double sum=0;
    for(int y=0;y<area.getHeight();++y)for(int x=0;x<area.getWidth();++x)sum+=pixels.getPixelColour(x,y).getPerceivedBrightness();
    const float mean=float(sum/double(juce::jmax(1,area.getWidth()*area.getHeight())));
    for(int y=0;y<area.getHeight();++y) {
        // A band can fade out towards its bottom (the ceiling into the walls).
        const float row=float(y)/float(juce::jmax(1,area.getHeight()-1));
        const float weight=row<fadeFrom ? 1.f : juce::jmax(0.f,1.f-(row-fadeFrom)/juce::jmax(.001f,1.f-fadeFrom));
        const float k=look.amount*weight;
        for(int x=0;x<area.getWidth();++x) {
            const auto c=pixels.getPixelColour(x,y);
            const float shade=juce::jlimit(.12f,1.6f,.62f+look.contrast*(c.getPerceivedBrightness()-mean));
            const auto mix=[&](float photo,float surface){return juce::jlimit(0.f,1.f,photo*(1.f-k)+k*surface*shade);};
            pixels.setPixelColour(x,y,juce::Colour::fromFloatRGBA(mix(c.getFloatRed(),look.r),mix(c.getFloatGreen(),look.g),mix(c.getFloatBlue(),look.b),1.f));
        }
    }
}

// The material's pattern in the surface's own direction. On the floor,
// horizontal courses bunch towards the far edge and vertical lines converge
// on a point above it.
inline void paintPattern(juce::Graphics& g,juce::Rectangle<float> area,int material,bool floor,float scale) {
    if(area.isEmpty() || material<0)return;
    juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(area.toNearestInt());
    juce::Random random(int64_t(material*7919+int(area.getY())));
    const auto rows=[&](float spacing,juce::Colour colour,float thickness){
        g.setColour(colour);
        if(!floor){for(float y=area.getY()+spacing;y<area.getBottom();y+=spacing)g.fillRect(area.getX(),y,area.getWidth(),thickness*scale);return;}
        const int count=juce::jmax(2,int(area.getHeight()/spacing));
        for(int i=1;i<count;++i){const float t=float(i)/float(count);g.fillRect(area.getX(),area.getY()+area.getHeight()*t*t,area.getWidth(),thickness*scale*(.5f+t));}
    };
    const auto columns=[&](float spacing,juce::Colour colour,float thickness){
        g.setColour(colour);
        if(!floor){for(float x=area.getX()+spacing;x<area.getRight();x+=spacing)g.fillRect(x,area.getY(),thickness*scale,area.getHeight());return;}
        const float cx=area.getCentreX(),depth=area.getHeight()*1.6f;
        for(float x=cx-area.getWidth();x<=cx+area.getWidth();x+=spacing*.6f)
            g.drawLine(x,area.getY(),cx+(x-cx)*(depth+area.getHeight())/depth,area.getBottom(),thickness*scale);
    };
    const auto specks=[&](float density,juce::Colour colour,float size){
        g.setColour(colour);const int count=int(area.getWidth()*area.getHeight()*density);
        for(int i=0;i<count;++i)g.fillRect(area.getX()+random.nextFloat()*area.getWidth(),area.getY()+random.nextFloat()*area.getHeight(),size*scale,size*scale);
    };
    const float unit=juce::jmax(8.f,22.f*scale);
    const auto dark=[](float a){return juce::Colours::black.withAlpha(a);};
    switch(material) {
        case 0: specks(1.f/160.f,dark(.18f),1.2f);specks(1.f/420.f,juce::Colours::white.withAlpha(.08f),1.f);break; // Concrete
        case 1: { // Brick: running bond with light mortar
            const float h=unit*.8f,w=unit*2.2f;g.setColour(juce::Colour(0xffd0bca4).withAlpha(.35f));int row=0;
            for(float y=area.getY();y<area.getBottom();y+=h,++row) {
                g.fillRect(area.getX(),y,area.getWidth(),1.3f*scale);
                for(float x=area.getX()-(row%2 ? w*.5f : 0.f);x<area.getRight();x+=w)g.fillRect(x,y,1.3f*scale,h);
            }
            break;
        }
        case 2: specks(1.f/600.f,dark(.08f),1.f);break;                       // Plaster
        case 3: columns(unit*5.5f,dark(.22f),1.2f);break;                     // Gypsum board seams
        case 4: rows(unit*.65f,dark(.32f),1.f);break;                         // Wood floor planks
        case 5: columns(unit*1.4f,dark(.25f),1.2f);break;                     // Wood panel
        case 6: specks(1.f/70.f,juce::Colours::white.withAlpha(.05f),1.f);specks(1.f/70.f,dark(.12f),1.f);break; // Carpet pile
        case 7: { // Heavy curtain folds
            const float fold=unit*1.3f;
            for(float x=area.getX();x<area.getRight();x+=fold) {
                juce::ColourGradient shade(dark(.32f),x,0.f,dark(.32f),x+fold,0.f,false);
                shade.addColour(.5,juce::Colours::white.withAlpha(.07f));
                g.setGradientFill(shade);g.fillRect(x,area.getY(),fold,area.getHeight());
            }
            break;
        }
        case 8: rows(unit*2.2f,dark(.28f),1.2f);columns(unit*2.2f,dark(.28f),1.2f);specks(1.f/220.f,dark(.1f),1.f);break; // Acoustic tile
        case 9: { // Glass reflections
            g.setColour(juce::Colours::white.withAlpha(.09f));
            for(float x=area.getX()-area.getHeight();x<area.getRight();x+=unit*6.f)g.drawLine(x,area.getBottom(),x+area.getHeight()*.6f,area.getY(),unit*.9f);
            break;
        }
        default: break;
    }
}

// The raven relief on the back wall keeps its photographed look under every
// wall material. Its silhouette is found once on the full-resolution photo:
// the wall's slats only change brightness across (vertical edges), the
// feathers also change it up and down. Those up-down edges, closed, with
// holes filled and only the largest shape kept, give the relief; the edge is
// softened by a few pixels. Returns a single-channel mask the size of photo
// (255 = keep the photo), searched in the region around the relief.
inline juce::Image emblemMask(const juce::Image& photo) {
    juce::Image mask(juce::Image::SingleChannel,juce::jmax(1,photo.getWidth()),juce::jmax(1,photo.getHeight()),true);
    if(!photo.isValid())return mask;
    const int x0=int(photo.getWidth()*.40f),x1=int(photo.getWidth()*.60f),y0=int(photo.getHeight()*.025f),y1=int(photo.getHeight()*.42f);
    const int w=x1-x0,h=y1-y0;if(w<8 || h<8)return mask;
    std::vector<float> lum(size_t(w*h)),tmp(size_t(w*h));
    const auto at=[w](int x,int y){return size_t(y*w+x);};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto c=photo.getPixelAt(x0+x,y0+y);lum[at(x,y)]=(c.getFloatRed()+c.getFloatGreen()+c.getFloatBlue())/3.f;}
    // Light 1-4-6-4-1 smoothing, horizontal then vertical.
    constexpr float kernel[5]{1/16.f,4/16.f,6/16.f,4/16.f,1/16.f};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){float v=0;for(int k=-2;k<=2;++k)v+=kernel[k+2]*lum[at(juce::jlimit(0,w-1,x+k),y)];tmp[at(x,y)]=v;}
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){float v=0;for(int k=-2;k<=2;++k)v+=kernel[k+2]*tmp[at(x,juce::jlimit(0,h-1,y+k))];lum[at(x,y)]=v;}
    std::vector<uint8_t> shape(size_t(w*h)),work(size_t(w*h));
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)shape[at(x,y)]=std::abs(lum[at(x,y)]-lum[at(x,juce::jmax(0,y-1))])>.035f;
    // Closing: dilate then erode with a 17 x 17 square (separable).
    const auto morph=[&](bool dilate){
        constexpr int r=8;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){bool v=!dilate;for(int k=-r;k<=r;++k){const bool s=shape[at(juce::jlimit(0,w-1,x+k),y)];v=dilate?(v||s):(v&&s);}work[at(x,y)]=v;}
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){bool v=!dilate;for(int k=-r;k<=r;++k){const bool s=work[at(x,juce::jlimit(0,h-1,y+k))];v=dilate?(v||s):(v&&s);}shape[at(x,y)]=v;}
    };
    morph(true);morph(false);
    // Fill holes: everything not reachable from the border is inside.
    std::vector<uint8_t> outside(size_t(w*h));std::vector<int> queue;queue.reserve(size_t(w*h));
    const auto seed=[&](int x,int y){if(!shape[at(x,y)] && !outside[at(x,y)]){outside[at(x,y)]=1;queue.push_back(int(at(x,y)));}};
    for(int x=0;x<w;++x){seed(x,0);seed(x,h-1);}
    for(int y=0;y<h;++y){seed(0,y);seed(w-1,y);}
    for(size_t i=0;i<queue.size();++i){const int x=queue[i]%w,y=queue[i]/w;if(x>0)seed(x-1,y);if(x<w-1)seed(x+1,y);if(y>0)seed(x,y-1);if(y<h-1)seed(x,y+1);}
    // Keep the largest connected shape.
    std::vector<int> label(size_t(w*h),0);int best=0,bestSize=0,next=0;
    for(int start=0;start<w*h;++start) {
        if(outside[size_t(start)] || label[size_t(start)])continue;
        queue.clear();queue.push_back(start);label[size_t(start)]=++next;
        for(size_t i=0;i<queue.size();++i) {
            const int x=queue[i]%w,y=queue[i]/w;
            const auto visit=[&](int nx,int ny){const auto j=at(nx,ny);if(!outside[j] && !label[j]){label[j]=next;queue.push_back(int(j));}};
            if(x>0)visit(x-1,y);if(x<w-1)visit(x+1,y);if(y>0)visit(x,y-1);if(y<h-1)visit(x,y+1);
        }
        if(int(queue.size())>bestSize){bestSize=int(queue.size());best=next;}
    }
    // Soft edge: two 5 x 5 box blurs.
    for(int i=0;i<w*h;++i)tmp[size_t(i)]=label[size_t(i)]==best && best ? 1.f : 0.f;
    for(int pass=0;pass<2;++pass) {
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){float v=0;for(int k=-2;k<=2;++k)v+=tmp[at(juce::jlimit(0,w-1,x+k),y)];lum[at(x,y)]=v/5.f;}
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){float v=0;for(int k=-2;k<=2;++k)v+=lum[at(x,juce::jlimit(0,h-1,y+k))];tmp[at(x,y)]=v/5.f;}
    }
    juce::Image::BitmapData out(mask,juce::Image::BitmapData::writeOnly);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)*out.getPixelPointer(x0+x,y0+y)=uint8_t(juce::jlimit(0,255,juce::roundToInt(tmp[at(x,y)]*255.f)));
    return mask;
}
// Puts back the photographed pixels where keep (single channel, the size of
// image) is set: original is the scenery before materials were applied.
inline void restoreKept(juce::Image& image,const juce::Image& original,const juce::Image& keep) {
    if(!keep.isValid() || keep.getBounds()!=image.getBounds() || original.getBounds()!=image.getBounds())return;
    juce::Image::BitmapData k(keep,juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData o(original,juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData p(image,juce::Image::BitmapData::readWrite);
    for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x) {
        const float a=float(*k.getPixelPointer(x,y))/255.f;if(a<=0.f)continue;
        p.setPixelColour(x,y,p.getPixelColour(x,y).interpolatedWith(o.getPixelColour(x,y),a));
    }
}
// `image` already holds the scenery drawn into `photo` (the rectangle the
// photo occupies, which may extend past the image). Recolours and patterns
// the three surfaces in place.
inline void applySpaceMaterials(juce::Image& image,juce::Rectangle<float> photo,const SpaceSurfaces& surfaces) {
    if(!surfaces.on || photo.isEmpty() || !image.isValid())return;
    const float h=photo.getHeight(),scale=juce::jmax(.5f,h/412.f);
    const auto band=[&](float top,float bottom){return juce::Rectangle<float>(photo.getX(),photo.getY()+h*top,photo.getWidth(),h*(bottom-top));};
    const auto walls=band(.02f,.61f),floor=band(.64f,1.f),ceiling=band(0.f,.09f);
    recolourSurface(image,walls.toNearestInt(),surfaces.walls,false,true);
    recolourSurface(image,floor.toNearestInt(),surfaces.floor,true,false);
    // The photographed ceiling is a 2 % light strip: show the ceiling material
    // there and as a band fading into the top of the walls.
    recolourSurface(image,ceiling.toNearestInt(),surfaces.ceiling,false,false,.25f);
    juce::Graphics g(image);
    paintPattern(g,walls,surfaces.walls,false,scale);
    paintPattern(g,floor,surfaces.floor,true,scale);
    paintPattern(g,band(0.f,.025f),surfaces.ceiling,false,scale);
}
}
