/* port: installed-file regression audit and rendered model catalogue.
   omt1_models_test <game folder> <output folder>; see docs/marlin-model.md. */
#include "OMediaDataBase.h"
#include "OMediaCanvas.h"
#include "OMediaDisplay.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaVPElement.h"
#include "OMediaBackground.h"
#include "OMedia3DWorld.h"
#include "OMedia3DRasterizerDesc.h"
#include "OMedia3DGrannyModel.h"
#include "granny_internal.h"
#include <SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <set>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern char **omt_argv;
extern int omt_argc;
static void files(const std::string &dir, std::vector<std::string> &out)
{
    DIR *d = opendir(dir.c_str());
    if (!d) {fprintf(stderr, "cannot read %s\n", dir.c_str()); exit(1);}
    for (dirent *e = readdir(d); e; e = readdir(d))
    {
        if (e->d_name[0] == '.') continue;
        std::string p = dir + "/" + e->d_name;
        struct stat st;
        if (stat(p.c_str(), &st)) exit(1);
        if (S_ISDIR(st.st_mode)) files(p, out);
        else if (p.size() > 4 && !strcasecmp(p.c_str() + p.size() - 4, ".grn")) out.push_back(p);
    }
    closedir(d);
}
static bool finite(float x) {return x == x && x >= -1.e30f && x <= 1.e30f;}
void omt_main(void)
{
    using namespace granny_internal;
    if (omt_argc != 3) {fprintf(stderr, "usage: omt1_models_test <game folder> <output folder>\n"); exit(2);}
    std::string root = omt_argv[1], out = omt_argv[2];
    mkdir(out.c_str(), 0755);
    std::vector<std::string> paths, models;
    files(root + "/models", paths);
    std::sort(paths.begin(), paths.end());
    OMedia3DGrannyModel::init(root);
    unsigned failures = 0, animations = 0;
    for (size_t i = 0; i < paths.size(); i++)
    {
        unsigned previous_failures = failures;
        granny_result err = 0;
        std::vector<std::string> warnings;
        FileData *f = grn_load(paths[i], &err, &warnings);
        if (!f) {printf("FAIL %s: load %d\n", paths[i].c_str(), err); failures++; continue;}
        size_t vertices = 0, indices = 0, bindings = 0, max_influences = 0, nonfinite_normals = 0;
        for (size_t m = 0; m < f->meshes.size(); m++)
        {
            const Mesh &mesh = f->meshes[m];
            vertices += mesh.nrender; bindings += mesh.bindings.size();
            for (size_t s = 0; s < mesh.states.size(); s++)
            {
                const MeshState &state = mesh.states[s];
                indices += state.tris.size();
                for (size_t t = 0; t < state.tris.size(); t++) if (state.tris[t] >= mesh.nrender) failures++;
                if (state.texture != 0xffffffffu && state.texture >= f->textures.size()) failures++;
            }
            for (size_t v = 0; v < mesh.nsource; v++)
            {
                size_t p = mesh.wofs[v];
                if (p >= mesh.weights.size()) {failures++; continue;}
                size_t n = mesh.weights[p];
                max_influences = std::max(max_influences, n);
                if (p + 1 + n * 2 > mesh.weights.size()) {failures++; continue;}
                for (size_t k = 0; k < n; k++) if (mesh.weights[p + 1 + 2*k] >= mesh.bindings.size()) failures++;
            }
            for (size_t k = 0; k < mesh.pos.size(); k++) {if (!finite(mesh.pos[k]) || !finite(mesh.uv[k])) failures++; if (!finite(mesh.nrm[k])) nonfinite_normals++;}
        }
        for (size_t w = 0; w < warnings.size(); w++)
        {
            // Embedded RGB decode is not used by OMT: model .omt supplies the texture.
            bool pixels = warnings[w].find("texture not decoded") != std::string::npos;
            printf("%s %s\n", pixels ? "NOTE" : "FAIL", warnings[w].c_str());
            if (!pixels) failures++;
        }
        if (nonfinite_normals) printf("NOTE %s: %lu nonfinite source normal components (preserved)\n", paths[i].c_str(), (unsigned long)nonfinite_normals);
        printf("FILE %s meshes=%lu vertices=%lu indices=%lu bones=%lu bindings=%lu max-influences=%lu materials=%lu tracks=%lu\n",
            paths[i].c_str(), (unsigned long)f->meshes.size(), (unsigned long)vertices, (unsigned long)indices,
            (unsigned long)f->bones.size(), (unsigned long)bindings, (unsigned long)max_influences,
            (unsigned long)f->textures.size(), (unsigned long)f->tracks.size());
        if (previous_failures != failures) printf("FAIL audit %s: %u problems\n", paths[i].c_str(), failures - previous_failures);
        if (!f->meshes.empty()) models.push_back(paths[i]);
        if (!f->tracks.empty()) animations++;
        delete f;
    }
    std::string frames;
    for (size_t i = 0; i < models.size(); i++) {char n[32]; snprintf(n, sizeof n, "%lu,", (unsigned long)(i+1)); frames += n;}
    setenv("OMT_SCREENSHOT", (out + "/model.bmp").c_str(), 1);
    setenv("OMT_SHOTS", frames.c_str(), 1);
    setenv("OMT_FRAMES", "100000", 1);
    OMediaDataBase::set_cache_size(0);
    omd_REGISTERCLASS(OMediaCanvas);
    omd_REGISTERCLASS(OMedia3DMaterial);
    OMediaDirectX::install(OMediaDirectX::omcdx_Video);
    OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
    OMediaVideoMode *mode = NULL;
    OMediaVideoDriver &drv = dev->get_video_drivers()->front();
    for (size_t i = 0; i < drv.modes.size(); i++)
        if (drv.modes[i].width == 640 && drv.modes[i].height == 480 && drv.modes[i].depth == 16) mode = &drv.modes[i];
    dev->setmode(mode, 0);
    OMediaDoubleCanvas *dc = new OMediaDoubleCanvas;
    OMediaDisplay *display = new OMediaDisplay(NULL, dc, "model audit");
    OMedia2DWorld *world = new OMedia2DWorld;
    world->set_framerate_mode(omfrc_Max);
    OMediaRGBColor bg(0x3333, 0x3333, 0x3333);
    ((OMediaColorBackground*)world->getbackground())->setcolor(&bg);
    OMediaViewPort *vp = new OMediaViewPort(world);
    OMediaRect bounds(0, 0, 640, 480);
    vp->setbounds(&bounds); vp->setcanvas(dc);
    OMedia3DAbstractRasterizer *ras = OMedia3DRasterizerManager::get_default()->build_rasterizer(NULL, NULL, 0);
    ras->set_texture_filtering(3);
    OMedia3DWorld *w3 = new OMedia3DWorld;
    w3->set_framerate_mode(omfrc_Max); w3->setbackground(NULL);
    w3->focal = 400; w3->near_z = 1; w3->far_z = 10000; w3->rasterizer = ras;
    w3->ambient.setred8bits(255); w3->ambient.setgreen8bits(255); w3->ambient.setblue8bits(255);
    OMediaViewPort *vp3 = new OMediaViewPort(w3);
    vp3->setbounds(&bounds);
    OMediaVPElement *vpe = new OMediaVPElement;
    vpe->set_viewport(vp3); vpe->link(world); vpe->place(0, 0, 100);
    FILE *manifest = fopen((out + "/thumbnails.tsv").c_str(), "w");
    if (!manifest) exit(1);
    for (size_t i = 0; i < models.size(); i++)
    {
        OMedia3DGrannyModel model;
        std::string base = models[i].substr(0, models[i].size()-4);
        bool embedded = base.find("_model") == std::string::npos && base.find("_Model") == std::string::npos;
        if (embedded) base = base.substr(0, base.rfind('_')) + "_model";
        if (!model.load(base)) {printf("FAIL %s: %s\n", base.c_str(), model.get_error().c_str()); failures++; continue;}
        // Sample an available idle through the same skinning and pose-copy path as the game.
        std::string idle = base;
        size_t suffix = idle.rfind('_');
        if (suffix != std::string::npos) idle.resize(suffix);
        idle += "_idle1.grn";
        struct stat idle_stat;
        if (embedded) idle = models[i];
        if (!stat(grn_find_file(idle).c_str(), &idle_stat) && !model.play(idle, 1, 9999, 0))
            {printf("FAIL idle %s\n", idle.c_str()); failures++;}
        OMedia3DGrannyModel::advance_time(0.5);
        model.update_pose();
        std::vector<OMedia3DPoint> &vs = *model.get_shape()->get_vertices();
        float lo[3] = {1.e30f,1.e30f,1.e30f}, hi[3] = {-1.e30f,-1.e30f,-1.e30f};
        for (size_t v = 0; v < vs.size(); v++)
        {
            float p[3] = {vs[v].x, vs[v].y, vs[v].z};
            for (int k = 0; k < 3; k++) {if (!finite(p[k])) failures++; lo[k] = std::min(lo[k],p[k]); hi[k] = std::max(hi[k],p[k]);}
        }
        float extent = std::max(std::max(hi[0]-lo[0],hi[1]-lo[1]),hi[2]-lo[2]);
        if (extent <= 0) {printf("FAIL empty extent %s\n", base.c_str()); failures++; extent = 1;}
        float scale = 300 / extent;
        model.get_shape()->scale(scale, scale, scale);
        OMedia3DShapeElement element;
        element.setshape(model.get_shape()); element.link(w3);
        element.place(-(lo[0]+hi[0])*scale/2, -(lo[1]+hi[1])*scale/2, 600-(lo[2]+hi[2])*scale/2);
        std::set<OMedia3DMaterial*> materials;
        std::vector<OMedia3DPolygon> &ps = *model.get_shape()->get_polygons();
        for (size_t p = 0; p < ps.size(); p++)
        {
            OMedia3DMaterial *mat = ps[p].get_material();
            if (mat)
            {
                materials.insert(mat);
                if (mat->mode == ommm_TexturedLight && !mat->get_texture())
                    {printf("FAIL missing canvas %s\n", base.c_str()); failures++; break;}
            }
            else if (base.find("_model") != std::string::npos) {printf("FAIL missing material %s\n", base.c_str()); failures++; break;}
        }
        SDL_Event ev; while (SDL_PollEvent(&ev)) {}
        w3->update(); world->update();
        fprintf(manifest, "model-%lu.bmp\t%s\n", (unsigned long)(i+1), models[i].substr(root.size()+1).c_str());
        printf("DRAW %s bound-materials=%lu\n", base.c_str(), (unsigned long)materials.size());
    }
    fclose(manifest);
    // Assemble the captures in manifest order; SDL keeps this target independent of
    // image tools. model-sheet.py adds filenames and writes the labelled PNG.
    SDL_Surface *sheet = SDL_CreateRGBSurface(0, 8*240, ((models.size()+7)/8)*180, 32,
        0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    if (!sheet) {fprintf(stderr, "sheet allocation failed\n"); failures++;}
    else
    {
        SDL_FillRect(sheet, NULL, SDL_MapRGB(sheet->format, 48, 48, 48));
        for (size_t i = 0; i < models.size(); i++)
        {
            char name[64]; snprintf(name, sizeof name, "/model-%lu.bmp", (unsigned long)(i+1));
            SDL_Surface *thumb = SDL_LoadBMP((out+name).c_str());
            if (!thumb) {failures++; continue;}
            SDL_Rect cell = {int(i%8)*240, int(i/8)*180, 240, 180};
            if (SDL_BlitScaled(thumb, NULL, sheet, &cell)) failures++;
            SDL_FreeSurface(thumb);
        }
        if (SDL_SaveBMP(sheet, (out+"/contact.bmp").c_str())) failures++;
        SDL_FreeSurface(sheet);
    }
    delete w3; delete world; delete ras; delete display;
    printf("MODEL SUMMARY: files=%lu models=%lu animations=%u failures=%u\n", (unsigned long)paths.size(), (unsigned long)models.size(), animations, failures);
    if (failures) exit(1);
}
