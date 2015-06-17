#include "CameraLoader.h"
#include "VoxelModel.h"
#include "morton_code.h"
#include <trimesh2/TriMesh.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QtDebug>

#include <QImage>

#include <stdlib.h>
#include <iostream>

static void visual_hull(recon::VoxelModel& model, const QList<recon::Camera>& cameras);
static void plane_sweep(recon::VoxelModel& model, const QList<recon::Camera>& cameras);
static void save_model(const QString& path, const recon::VoxelModel& model);
static bool check_photo_consistency(const QList<uint32_t>& pixels, const QList<uint32_t>& pixbounds, uint32_t& vcolor);

int main(int argc, char* argv[])
{
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("voxel");
  QCoreApplication::setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("Model Generator");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("bundle", "Input bundle file");

  parser.process(app);

  const QStringList args = parser.positionalArguments();
  if (args.count() < 1) {
    std::cout << "Bundle path?\n";
    return 0;
  }

  const QString bundlePath = args.at(0);

  recon::CameraLoader loader;
  if (!loader.load_from_nvm(bundlePath)) {
    qDebug() << "Cannot load cameras from " << bundlePath;
    return 1;
  }

  QList<recon::Camera> cameras = loader.cameras();

  QStringList mask_paths;
  mask_paths.reserve(cameras.size());
  for (recon::Camera cam : cameras) {
    QString path =  cam.imagePath();
    QString rootname = path.section(QDir::separator(), 0, -3, QString::SectionIncludeLeadingSep);
    QString filename = path.section(QDir::separator(), -1);
    QString mpath = rootname + QString(QDir::separator()) + "masks" + QString(QDir::separator()) + filename;
    cam.setMaskPath(mpath);
  }

  //for (recon::Camera cam : cameras) {
  //  qDebug() << "mask path = " << cam.maskPath();
  //}

  recon::VoxelModel model(7, loader.model_boundingbox());
  visual_hull(model, cameras);
  plane_sweep(model, cameras);
  save_model("model.ply", model);

  return 0;
}

static void visual_hull(recon::VoxelModel& model, const QList<recon::Camera>& cameras)
{
  using recon::AABox;
  using recon::VoxelData;
  using recon::Camera;
  using recon::mat4;
  using recon::vec3;
  using recon::vec4;
  using recon::morton_decode;

  qDebug() << "visual_hull: initialize";

  // Initialize
  for (uint64_t m = 0; m < model.size(); ++m) {
    uint32_t val = 0;
    val |= VoxelData::VISUALHULL_FLAG;
    model[m].flag = val;
  }

  // Visual Hull
  for (Camera cam : cameras) {
    qDebug() << "visual_hull: camera iteration";
    QImage mask = QImage(cam.maskPath());

    mat4 extrinsic = cam.extrinsic();
    mat4 intrinsic = cam.intrinsicForImage(mask.width(), mask.height());
    mat4 transform = intrinsic * extrinsic;

    for (uint64_t m = 0; m < model.size(); ++m) {
      VoxelData& voxel = model[m];
      if ((voxel.flag & VoxelData::VISUALHULL_FLAG) == 0) {
        continue;
      }

      AABox vbox = model.boundingbox(m);
      vec3 pt = vbox.center();
      pt = proj_vec3(transform * vec4(pt, 1.0f));

      QPoint pt2d = QPoint((float)pt.x(), (float)pt.y());
      if (mask.valid(pt2d)) {
        if (qGray(mask.pixel(pt2d)) < 10) {
          voxel.flag &= ~VoxelData::VISUALHULL_FLAG; // mark invisible
          // mark neighborhood
          uint32_t x, y, z;
          morton_decode(m, x, y, z);
          VoxelData* neighbor;

          if ((neighbor = model.get(x+1, y, z))) {
            neighbor->flag |= VoxelData::SURFACE_FLAG;
          }
          if ((neighbor = model.get(x-1, y, z))) {
            neighbor->flag |= VoxelData::SURFACE_FLAG;
          }
          if ((neighbor = model.get(x, y+1, z))) {
            neighbor->flag |= VoxelData::SURFACE_FLAG;
          }
          if ((neighbor = model.get(x, y-1, z))) {
            neighbor->flag |= VoxelData::SURFACE_FLAG;
          }
          if ((neighbor = model.get(x, y, z+1))) {
            neighbor->flag |= VoxelData::SURFACE_FLAG;
          }
          if ((neighbor = model.get(x, y, z-1))) {
            neighbor->flag |= VoxelData::SURFACE_FLAG;
          }
        }
      } else {
        voxel.flag &= ~VoxelData::VISUALHULL_FLAG;
      }
    }
  }

  qDebug() << "visual_hull: finalize";

  // Finalize
  for (uint64_t m = 0; m < model.size(); ++m) {
    if ((model[m].flag & VoxelData::VISUALHULL_FLAG) == 0) {
      model[m].flag = 0;
    }
  }
}

static void plane_sweep(recon::VoxelModel& model, const QList<recon::Camera>& cameras)
{
  using recon::Camera;
  using recon::VoxelData;
  using recon::vec3;
  using recon::mat4;
  using recon::vec4;
  using recon::AABox;
  using recon::Ray;

  QList<QImage> images;
  images.reserve(cameras.size());
  for (Camera cam : cameras) {
    images.append(QImage(cam.imagePath()));
  }
  int cam_n = cameras.size();

  for (int iterations = 1; iterations; --iterations) {
    for (uint64_t morton = 0, morton_count = model.size(); morton < morton_count; ++morton) {
      VoxelData& voxel = model[morton];
      if ((voxel.flag & VoxelData::SURFACE_FLAG) == 0)
        continue;

      for (int cam_id = 0; cam_id < cam_n; ++cam_id) {
        Camera cam = cameras[cam_id];
        QImage image = images[cam_id];

        if (!model.check_visibility(cam.center(), morton)) {
          continue;
        }

        // project 8 corners of voxel onto the image, compute the bounding rectangle
        vec3 minpt, maxpt;
        {
          int width = image.width(), height = image.height();
          mat4 proj = cam.intrinsicForImage(width, height) * cam.extrinsic();
          AABox vbox = model.boundingbox(morton);
          vec3 corners[8] = {
            vbox.corner0(),
            vbox.corner1(),
            vbox.corner2(),
            vbox.corner3(),
            vbox.corner4(),
            vbox.corner5(),
            vbox.corner6(),
            vbox.corner7()
          };
          maxpt = minpt = proj_vec3(proj * vec4(corners[0], 1.0f));
          for (int i = 1; i < 8; ++i) {
            vec3 pt = proj_vec3(proj * vec4(corners[i], 1.0f));
            minpt = min(minpt, pt);
            maxpt = max(maxpt, pt);
          }
          clamp(minpt, vec3::zero(), vec3((float)width, (float)height, 0.0f));
          clamp(maxpt, vec3::zero(), vec3((float)width, (float)height, 0.0f));
        }

        // compute color
        vec3 color = vec3::zero();
        float count = 0.0f;
        for (int px = (float)minpt.x(); px < (float)maxpt.x(); ++px) {
          for (int py = (float)minpt.y(); py < (float)maxpt.y(); ++py) {
            QRgb c = image.pixel(px, py);
            color = color + vec3(qRed(c), qGreen(c), qBlue(c));
            count += 1.0f;
          }
        }
        color = color / count;
        uint32_t ucolor = qRgb((float)color.x(), (float)color.y(), (float)color.z());
        float udepth = 1.0f / (float)maxpt.z();

        //if ((voxel.flag & VoxelData::VOXELCOLOR_1_FLAG) == 0) {
          voxel.color_depth = udepth;
          voxel.color = ucolor;
          voxel.flag |= VoxelData::VOXELCOLOR_1_FLAG;
        //} else if (voxel.color_depth > udepth){
        //  voxel.color_depth = udepth;
        //  voxel.color = ucolor;
        //}
      }
    }
  }

  /*
  TODO
  update_flag = true
  while update_flag == true do
    update_flag = false
    for each voxel do
      continue if voxel is not surface voxel
      for each camera do
        continue if voxel is already computed with the camera
        check photo consistency
        if not consistent then
          carve the voxel (its flag becomes zero)
          mark neighboring voxels as surface voxels
          update_flag = true
        endif
      end
    end
  done
  */
  // https://gist.github.com/davll/86633cf34567e6852820#file-voxelcolor-cpp-L168

  // Finalize
  //for (uint64_t m = 0; m < model.size(); ++m) {
  //  if ((model[m].flag & VoxelData::VOXELCOLOR_1_FLAG) == 0) {
  //    model[m].flag = 0;
  //  }
  //}
}

static bool check_photo_consistency(const QList<uint32_t>& pixels, const QList<uint32_t>& pixbounds, uint32_t& vcolor)
{
  return true;
}

static void save_model(const QString& path, const recon::VoxelModel& model)
{
  using recon::AABox;
  using recon::VoxelData;

  uint64_t count = 0;
  for (uint64_t m = 0; m < model.size(); ++m) {
    if ((model[m].flag & VoxelData::SURFACE_FLAG) != 0)
      count++;
  }

  trimesh::TriMesh mesh;
  mesh.vertices.reserve(8 * count);
  mesh.colors.reserve(8 * count);
  mesh.faces.reserve(6 * 2 * count);

  uint64_t vid = 0;
  for (uint64_t m = 0; m < model.size(); ++m) {
    VoxelData v = model[m];
    if ((v.flag & VoxelData::SURFACE_FLAG) == 0)
      continue;

    AABox vbox = model.boundingbox(m);
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

    trimesh::Color vcolor(qRed(v.color), qGreen(v.color), qBlue(v.color));

    for (int i = 0; i < 8; ++i) {
      mesh.vertices.push_back(pt[i]);
      mesh.colors.push_back(vcolor);
    }
    for (int i = 0; i < 12; ++i)
      mesh.faces.push_back(face[i]);
  }

  mesh.need_tstrips();
  mesh.write(path.toUtf8().constData());
}
