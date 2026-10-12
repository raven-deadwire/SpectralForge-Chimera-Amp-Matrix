#pragma once
#include "CabEnclosureArtwork.h"

// Exercise the production tiling operation on both renderers. A software-only
// component snapshot cannot detect Direct2D wrapping a subsection's parent.
namespace cabEnclosureMaterialTests {
inline void checkRepeatedPixels(const juce::Image& tile,const juce::Image& repeated) {
    check(tile.isValid() && repeated.isValid(),"repeated grille texture is missing");
    check(repeated.getWidth()==tile.getWidth()*4 && repeated.getHeight()==tile.getHeight()*3,
        "repeated grille no longer has twelve whole source periods");
    const juce::Image::BitmapData source(tile,juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData copy(repeated,juce::Image::BitmapData::readOnly);
    check(source.pixelFormat==copy.pixelFormat && source.pixelStride==copy.pixelStride,
        "repeated grille changed its stored pixel format");
    for(int y=0;y<copy.height;++y)for(int x=0;x<copy.width;++x)
        check(std::memcmp(copy.getPixelPointer(x,y),source.getPixelPointer(x%source.width,y%source.height),
            size_t(source.pixelStride))==0,"repeated grille changed source phase or premultiplied pixel bytes");
}
inline void repeatedTextures() {
    using namespace spectralforge::cabEnclosureArt;
    check(!repeatGrilleTexture({}).isValid(),"missing custom grille produced a cached texture");
    for(bool native:{false,true}) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        for(auto format:{juce::Image::ARGB,juce::Image::RGB,juce::Image::SingleChannel}) {
            juce::Image tile(format,17,13,true,*type);
            {
                juce::Image::BitmapData pixels(tile,juce::Image::BitmapData::writeOnly);
                constexpr std::array<juce::uint8,6> alphas{{0,1,17,63,129,255}};
                for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x) {
                    const auto alpha=alphas[size_t((x+3*y)%int(alphas.size()))];
                    if(pixels.pixelFormat==juce::Image::ARGB) {
                        // Arbitrary valid premultiplied values expose a lossy
                        // unpremultiply/re-premultiply Colour round trip.
                        juce::PixelARGB pixel;
                        pixel.setARGB(alpha,juce::uint8((29*x+11*y)%(int(alpha)+1)),
                            juce::uint8((7*x+19*y)%(int(alpha)+1)),juce::uint8((13*x+5*y)%(int(alpha)+1)));
                        std::memcpy(pixels.getPixelPointer(x,y),&pixel,sizeof(pixel));
                    } else pixels.setPixelColour(x,y,juce::Colour(juce::uint8(17*x),juce::uint8(19*y),
                        juce::uint8(7*x+11*y),alpha));
                }
            }
            const auto repeated=repeatGrilleTexture(tile);
            checkRepeatedPixels(tile,repeated);
            std::array<juce::uint8,4> before{};int stride=0;
            {
                const juce::Image::BitmapData pixels(repeated,juce::Image::BitmapData::readOnly);
                stride=pixels.pixelStride;std::memcpy(before.data(),pixels.getPixelPointer(1,1),size_t(stride));
            }
            tile.clear(tile.getBounds(),juce::Colours::red);
            const juce::Image::BitmapData pixels(repeated,juce::Image::BitmapData::readOnly);
            check(std::memcmp(before.data(),pixels.getPixelPointer(1,1),size_t(stride))==0,
                "repeated grille still aliases its source storage");
        }
        // Non-uniform colour and alpha make an origin or scale change visible.
        // Check the production overlay's nearest-neighbour phase away from raw
        // tile edges, whose native coverage can differ after aggregation.
        juce::Image tile(juce::Image::ARGB,24,18,true,*type);
        {
            juce::Image::BitmapData pixels(tile,juce::Image::BitmapData::writeOnly);
            for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x)
                pixels.setPixelColour(x,y,juce::Colour(juce::uint8(40+45*(x/6)),juce::uint8(40+55*(y/6)),
                    juce::uint8(80+(x+y)%5),juce::uint8(40+55*(x/6))));
        }
        const auto repeated=repeatGrilleTexture(tile);
        Skin custom;custom.grille=tile;custom.grilleOverlay=tile;
        check(&custom.grilleTexture(false)==&custom.grille && &custom.grilleTexture(true)==&custom.grilleOverlay,
            "custom skin without rendering copies lost its raw grille fallback");
        for(float scale:{.25f,1.f,1.75f}) {
            juce::Image original(juce::Image::ARGB,192,128,true,*type);
            juce::Image cached(juce::Image::ARGB,192,128,true,*type);
            const juce::Rectangle<float> area(5.25f,7.75f,180.f,112.f);
            {juce::Graphics g(original);g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                fillMaterial(g,custom.grilleTexture(true),area,scale);}
            {juce::Graphics g(cached);g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                fillMaterial(g,repeated,area,scale);}
            const juce::Image::BitmapData a(original,juce::Image::BitmapData::readOnly);
            const juce::Image::BitmapData b(cached,juce::Image::BitmapData::readOnly);
            check(a.pixelStride==b.pixelStride,"grille renderers returned different target formats");
            int maximumError=0,samples=0;
            for(int y=10;y<117;++y)for(int x=8;x<183;++x) {
                const float phaseX=std::fmod((float(x)+.5f-area.getX())/scale,float(tile.getWidth()));
                const float phaseY=std::fmod((float(y)+.5f-area.getY())/scale,float(tile.getHeight()));
                if(phaseX<2.f || phaseX>float(tile.getWidth())-2.f
                    || phaseY<2.f || phaseY>float(tile.getHeight())-2.f)continue;
                const auto* expected=a.getPixelPointer(x,y);const auto* actual=b.getPixelPointer(x,y);
                for(int channel=0;channel<a.pixelStride;++channel)
                    maximumError=juce::jmax(maximumError,std::abs(int(expected[channel])-int(actual[channel])));
                ++samples;
            }
            std::cout<<"Repeated grille "<<(native ? "native" : "software")<<" scale="<<scale
                <<" phase_samples="<<samples<<" maximum_premultiplied_error="<<maximumError<<'\n';
            check(samples>1000 && maximumError<=2,"repeated grille shifted its rendered colour/alpha phase or physical scale");
            check(cached.getPixelAt(0,0).getAlpha()==0,"repeated grille escaped its material clip");
        }
    }
    std::cout<<"PASS repeated grille: exact raw pixels in three formats, independent storage, custom-skin fallback, "
        <<"12 native/software renders at three scales and a fractional origin\n";
}
inline void slantedRailContinuity() {
    using namespace spectralforge::cabEnclosureArt;
    // A bright, vertical piping stripe makes the edge's position observable
    // without depending on the photographed tolex grain or matching a golden
    // screenshot. Exercise the production painter, including its skin mapping.
    juce::SoftwareImageType fixtureStorage;
    juce::Image stripe(juce::Image::ARGB,32,32,true,fixtureStorage);
    {juce::Graphics g(stripe);g.fillAll(juce::Colour(0xff202020));
        g.setColour(juce::Colours::white);g.fillRect(12,0,8,32);}
    Skin skin;skin.perimeter.fill(stripe);
    skin.grille=skin.roof=skin.grilleOverlay=skin.header=stripe;
    skin.sourcePixelsPerMetre=32.f/.038f;
    check(skin.isValid(),"slanted rail continuity fixture is incomplete");
    int cases=0;
    for(bool native:{false,true})for(float ppm:{200.f,400.f,700.f})for(float dpi:{1.f,1.5f,2.f}) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        const juce::Rectangle<float> face(25.25f,31.75f,.74f*ppm,.78f*ppm);
        const juce::Point<float> offset(29.f,17.f);
        const auto placed=face.translated(offset.x,offset.y);
        const int width=int(std::ceil((placed.getRight()+20.f)*dpi));
        const int height=int(std::ceil((placed.getBottom()+20.f)*dpi));
        juce::Image absolute(juce::Image::ARGB,width,height,true,*type);
        juce::Image translated(juce::Image::ARGB,width,height,true,*type);
        {juce::Graphics g(absolute);g.addTransform(juce::AffineTransform::scale(dpi));
            paintFrame(g,skin,placed,ppm,true);}
        {juce::Graphics g(translated);g.addTransform(juce::AffineTransform::scale(dpi));
            g.addTransform(juce::AffineTransform::translation(offset));
            paintFrame(g,skin,face,ppm,true);}
        // Moving a cabinet's coordinates and moving its parent component must
        // preserve the physical piping position. CoreGraphics clips the path
        // and then paints transformed image tiles; equivalent coordinate/CTM
        // paths can have different edge coverage, so RGB equality is diagnostic
        // only. Compare piping centroids on both sides of the join instead.
        const float bend=(placed.getY()+placed.getHeight()*.46f)*dpi;
        const float rail=.038f*ppm*dpi;
        int maximumError=0;
        double errorSum=0.;int samples=0;
        float maximumStep=0.f,maximumTranslationShift=0.f,minimumJoinCoverage=1.f;
        for(bool right:{false,true}) {
            const float left=(right ? placed.getRight() : placed.getX())*dpi-(right ? rail : 0.f);
            const auto region=juce::Rectangle<float>(left-2.f,bend-6.f,rail+4.f,12.f)
                .getSmallestIntegerContainer().getIntersection(absolute.getBounds());
            for(int y=region.getY();y<region.getBottom();++y)for(int x=region.getX();x<region.getRight();++x) {
                const auto a=absolute.getPixelAt(x,y),b=translated.getPixelAt(x,y);
                const int error=juce::jmax(std::abs(int(a.getRed())-int(b.getRed())),
                    std::abs(int(a.getAlpha())-int(b.getAlpha())));
                maximumError=juce::jmax(maximumError,error);errorSum+=error;++samples;
            }
            const auto piping=[&](const juce::Image& rendered,int y) {
                float weight=0.f,moment=0.f;
                for(int x=region.getX();x<region.getRight();++x) {
                    const auto pixel=rendered.getPixelAt(x,y);
                    const float value=juce::jmax(0.f,float(pixel.getRed())-80.f)*pixel.getFloatAlpha();
                    weight+=value;moment+=value*(float(x)+.5f);
                }
                check(weight>40.f,"slanted side rail lost its visible piping at the join");
                return juce::Point<float>(moment/weight,weight);
            };
            const int aboveY=int(std::floor(bend))-2,belowY=int(std::ceil(bend))+2;
            for(int y:{aboveY,belowY})
                maximumTranslationShift=juce::jmax(maximumTranslationShift,
                    std::abs(piping(absolute,y).x-piping(translated,y).x));
            for(const auto* rendered:{&absolute,&translated}) {
                const auto above=piping(*rendered,aboveY);
                const auto below=piping(*rendered,belowY);
                maximumStep=juce::jmax(maximumStep,std::abs(above.x-below.x));
                const float baseline=juce::jmin(above.y,below.y);
                for(int y=int(std::floor(bend))-1;y<=int(std::ceil(bend))+1;++y)
                    minimumJoinCoverage=juce::jmin(minimumJoinCoverage,piping(*rendered,y).y/baseline);
            }
        }
        std::cout<<"Slanted rail "<<(native ? "native" : "software")<<" ppm="<<ppm<<" dpi="<<dpi
            <<" max_translation_error="<<maximumError<<" mean_translation_error="<<errorSum/double(samples)
            <<" piping_translation_shift_px="<<maximumTranslationShift
            <<" piping_step_px="<<maximumStep<<" join_coverage="<<minimumJoinCoverage<<'\n';
        // Half a device pixel tolerates raster coverage changes but rejects
        // the original multi-pixel texture/piping jump. The independent join
        // alignment and gap checks below also apply to each placement itself.
        check(maximumTranslationShift<=.5f,
            "slanted side rail piping moves when its parent component moves");
        check(maximumStep<=1.1f,"upper and lower side rail piping are horizontally misaligned");
        check(minimumJoinCoverage>.55f,"upper and lower side rails leave a visible gap at their join");
        ++cases;
    }
    check(cases==18,"slanted rail native/software scale matrix is incomplete");
    std::cout<<"PASS slanted side rails: 36 production renders, native/software, three camera densities and DPI scales, "
        <<"translation invariance and continuous piping at both joins\n";
}
inline void transmission() {
    using namespace spectralforge::cabEnclosureArt;
    for(bool bass:{false,true})for(bool native:{false,true}) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        juce::Image source(juce::Image::ARGB,16,12,true,*type);
        {
            juce::Graphics g(source);g.fillAll(juce::Colours::black);
            g.setColour(juce::Colour(0xff888888));g.fillRect(0,0,8,12);
        }
        const auto overlay=makeGrilleOverlay(source,bass);
        const auto wire=overlay.getPixelAt(3,6);
        check(wire.getAlpha()>100 && wire.getAlpha()<255 && overlay.getPixelAt(12,6).getAlpha()==0,
            "foreground grille lost transmissive apertures or wire texture");
        {juce::Graphics g(source);g.fillAll(juce::Colours::red);}
        for(float scale:{.25f,1.f,1.75f}) {
            juce::Image target(juce::Image::ARGB,112,76,true,*type);
            {juce::Graphics g(target);fillMaterial(g,overlay,{4.f,4.f,104.f,68.f},scale);}
            int minimum=255,maximum=0;
            for(int y=8;y<68;++y)for(int x=8;x<104;++x) {
                const auto pixel=target.getPixelAt(x,y);
                minimum=juce::jmin(minimum,int(pixel.getAlpha()));maximum=juce::jmax(maximum,int(pixel.getAlpha()));
                check(std::abs(int(pixel.getRed())-int(pixel.getGreen()))<=2,
                    "foreground grille tiling leaked coloured parent pixels");
            }
            check(minimum<10 && maximum>100 && maximum<255 && target.getPixelAt(0,0).getAlpha()==0,
                "foreground grille is opaque, missing or outside its baffle");
        }
    }
}
inline void run() {
    using namespace spectralforge::cabEnclosureArt;
    repeatedTextures();
    slantedRailContinuity();
    transmission();
    for(bool bass:{false,true})for(bool native:{false,true}) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        juce::Image source(juce::Image::ARGB,1254,1254,true,*type);
        const auto regions=sourceRegions(bass);
        {
            juce::Graphics g(source);
            g.fillAll(juce::Colours::red); // Forbidden parent pixels, outside the crops.
            g.setColour(juce::Colours::blue);g.fillRect(regions.grille);
            g.setColour(juce::Colours::green);g.fillRect(regions.roof);
            for(size_t i=1;i<regions.perimeter.size();i+=2) {
                g.setColour(juce::Colour(0xffd0a080));g.fillRect(regions.perimeter[i]);
            }
        }
        const auto skin=makeSkin(source,bass);
        check(skin.isValid(),"material fixture extraction failed");
        check(skin.grille.getBounds()==juce::Rectangle<int>(36, bass ? 66 : 72)
            && skin.roof.getBounds()==juce::Rectangle<int>(128,20),"material crop dimensions changed");
        checkRepeatedPixels(skin.grille,skin.repeatedGrille);
        checkRepeatedPixels(skin.grilleOverlay,skin.repeatedGrilleOverlay);
        for(size_t i=1;i<skin.perimeter.size();i+=2) {
            const auto& tile=skin.perimeter[i];
            check((i==topRail || i==bottomRail ? tile.getWidth() : tile.getHeight())==64,
                "rail material retained a whole stretched cabinet edge");
            for(float scale:{.25f,1.f,1.75f}) {
                juce::SoftwareImageType referenceStorage;
                juce::Image referenceTile(juce::Image::ARGB,tile.getWidth(),tile.getHeight(),true,referenceStorage);
                {juce::Graphics g(referenceTile);g.fillAll(juce::Colour(0xffd0a080));}
                juce::Image target(juce::Image::ARGB,192,128,true,*type);
                juce::Image reference(juce::Image::ARGB,192,128,true,*type);
                {juce::Graphics g(target);fillMaterial(g,tile,{4.f,4.f,184.f,120.f},scale);}
                {juce::Graphics g(reference);fillMaterial(g,referenceTile,{4.f,4.f,184.f,120.f},scale);}
                for(int y=8;y<120;++y)for(int x=8;x<184;++x) {
                    const auto pixel=target.getPixelAt(x,y);
                    const auto expected=reference.getPixelAt(x,y);
                    const bool noParent=pixel.getRed()<240 || pixel.getGreen()>32 || pixel.getBlue()>32;
                    const bool matches=std::abs(int(pixel.getRed())-int(expected.getRed()))<=2
                        && std::abs(int(pixel.getGreen())-int(expected.getGreen()))<=2
                        && std::abs(int(pixel.getBlue())-int(expected.getBlue()))<=2
                        && std::abs(int(pixel.getAlpha())-int(expected.getAlpha()))<=2;
                    check(noParent && matches,
                        "rail tiling leaked the original cabinet parent bitmap or lost reference coverage");
                }
            }
        }
        for(bool roof:{false,true}) {
            const auto& tile=roof ? skin.roof : skin.grille;
            const auto expected=roof ? juce::Colours::green : juce::Colours::blue;
            check(tile.getPixelAt(tile.getWidth()/2,tile.getHeight()/2)==expected,
                "repeating material still aliases the parent cabinet artwork");
            for(float scale:{.25f,1.f,1.75f}) {
                juce::SoftwareImageType referenceStorage;
                juce::Image referenceTile(juce::Image::ARGB,tile.getWidth(),tile.getHeight(),true,referenceStorage);
                {juce::Graphics g(referenceTile);g.fillAll(expected);}
                juce::Image target(juce::Image::ARGB,192,128,true,*type);
                juce::Image reference(juce::Image::ARGB,192,128,true,*type);
                {juce::Graphics g(target);fillMaterial(g,tile,{4.f,4.f,184.f,120.f},scale);}
                {juce::Graphics g(reference);fillMaterial(g,referenceTile,{4.f,4.f,184.f,120.f},scale);}
                // Compare with an independently owned tile rendered through
                // the same backend. This keeps native edge coverage strict
                // without treating premultiplied subpixel alpha as parent data.
                for(int y=8;y<120;++y)for(int x=8;x<184;++x) {
                    const auto pixel=target.getPixelAt(x,y);
                    const auto control=reference.getPixelAt(x,y);
                    const bool noParent=pixel.getRed()<240 || pixel.getGreen()>32 || pixel.getBlue()>32;
                    check(noParent
                        && std::abs(int(pixel.getRed())-int(control.getRed()))<=2
                        && std::abs(int(pixel.getGreen())-int(control.getGreen()))<=2
                        && std::abs(int(pixel.getBlue())-int(control.getBlue()))<=2
                        && std::abs(int(pixel.getAlpha())-int(control.getAlpha()))<=2,
                        "native/software material tiling leaked the parent cabinet or lost reference coverage");
                }
            }
        }
        // Copy ownership is also observable independently of a backend: edits
        // to the source must not inject artwork into either repeating tile.
        {
            juce::Graphics g(source);g.fillAll(juce::Colours::red);
        }
        check(skin.grille.getPixelAt(18,20)==juce::Colours::blue
            && skin.roof.getPixelAt(64,10)==juce::Colours::green,
            "repeating material still aliases the parent cabinet artwork");
        for(size_t i=1;i<skin.perimeter.size();i+=2)
            check(skin.perimeter[i].getPixelAt(10,10)==juce::Colour(0xffd0a080),
                "rail material still aliases the parent cabinet artwork");
    }
    std::cout<<"PASS enclosure materials: 72 native/software tiled renders, guitar/bass grille/roof/rails, "
        <<"three scales, no parent pixels and independent cropped storage\n";
}
}
