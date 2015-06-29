#include "VoxelModel.h"
#include "morton_code.h"
#include <QtGlobal>
#include <QColor>

namespace recon {

VoxelModel::VoxelModel(uint16_t lv, AABox model_box)
: level(lv)
, real_box(model_box)
, virtual_box()
, width(0x1 << lv)
, height(0x1 << lv)
, depth(0x1 << lv)
, morton_length(0x1ull << lv*3)
{
  if (level > 20) {
    qFatal("%s:%d: level is too high (level = %d)", __FILE__, __LINE__, level);
  }

  {
    float siz[3], vsiz;
    real_box.extent().store(siz);
    vsiz = fmaxf(siz[0], fmaxf(siz[1], siz[2]));

    Point3 start = real_box.minpos;
    Vec3 extent = Vec3(vsiz, vsiz, vsiz);
    virtual_box = AABox(start, start + extent);
  }
}

}
#include <trimesh2/TriMesh.h>
namespace recon {

void save_ply(const QString& path, const VoxelModel& model, const VoxelList& vlist)
{
  uint64_t count = vlist.count();

  trimesh::TriMesh mesh;
  mesh.vertices.reserve(8 * count);
  mesh.faces.reserve(6 * 2 * count);

  uint64_t vid = 0;
  for (uint64_t m : vlist) {
    AABox vbox = model.element_box(m);
    float x0, y0, z0, x1, y1, z1;

    x0 = (float)vbox.minpos.x();
    y0 = (float)vbox.minpos.y();
    z0 = (float)vbox.minpos.z();
    x1 = (float)vbox.maxpos.x();
    y1 = (float)vbox.maxpos.y();
    z1 = (float)vbox.maxpos.z();

    trimesh::point pt[] = {
      { x0, y0, z0 },
      { x1, y0, z0 },
      { x0, y1, z0 },
      { x1, y1, z0 },
      { x0, y0, z1 },
      { x1, y0, z1 },
      { x0, y1, z1 },
      { x1, y1, z1 }
    };
    trimesh::TriMesh::Face face[] = {
      { vid+0, vid+2, vid+1 },
      { vid+1, vid+2, vid+3 },
      { vid+0, vid+6, vid+2 },
      { vid+0, vid+4, vid+6 },
      { vid+0, vid+5, vid+4 },
      { vid+0, vid+1, vid+5 },
      { vid+1, vid+3, vid+5 },
      { vid+3, vid+7, vid+5 },
      { vid+3, vid+2, vid+6 },
      { vid+3, vid+6, vid+7 },
      { vid+4, vid+5, vid+7 },
      { vid+4, vid+7, vid+6 }
    };
    vid += 8;

    for (int i = 0; i < 8; ++i) {
      mesh.vertices.push_back(pt[i]);
    }
    for (int i = 0; i < 12; ++i)
      mesh.faces.push_back(face[i]);
  }

  mesh.need_tstrips();
  mesh.write(path.toUtf8().constData());
}

void save_ply(const QString& path, const VoxelModel& model, const QList<uint32_t>& colors)
{
  uint64_t count = 0;
  for (uint32_t c : colors)
    if (qAlpha(c) != 0)
      count++;

  trimesh::TriMesh mesh;
  mesh.vertices.reserve(count);
  mesh.colors.reserve(count);

  for (uint64_t m = 0; m < count; ++m) {
    uint32_t color = colors[m];
    if (qAlpha(color) == 0)
      continue;

    Point3 pos = model.element_box(m).center();

    trimesh::point pt = { (float)pos.x(), (float)pos.y(), (float)pos.z() };
    trimesh::Color c = { qRed(color), qGreen(color), qBlue(color) };

    mesh.vertices.push_back(pt);
    mesh.colors.push_back(c);
  }

  mesh.write(path.toUtf8().constData());
}

}
