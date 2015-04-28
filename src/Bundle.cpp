#include "Bundle.h"
#include <assert.h>
#include <QTextStream>

namespace recon {

Bundle::Bundle()
: m_CamerasNum(0), m_CamerasCapacity(0), m_Cameras(NULL)
{
}

Bundle::~Bundle()
{
  clear();
}

int Bundle::camera_count() const
{
  return m_CamerasNum;
}

Camera& Bundle::get_camera(int index)
{
  assert(index >= 0 && index < camera_count());
  return m_Cameras[index];
}

void Bundle::clear()
{
  if (m_Cameras) {
    free(m_Cameras);

    m_CamerasNum = 0;
    m_CamerasCapacity = 0;
    m_Cameras = NULL;
  }
}

bool Bundle::load_nvm(QTextStream& stream)
{
  QString token;

  // Magic
  stream >> token;
  if (token != "NVM_V3") {
    return false;
  }

  stream >> token;
  if (token == "FixedK") { // optional calibration
    float fx, cx, fy, cy;
    stream >> fx >> cx >> fy >> cy >> token;
  }

  int num_cameras, num_points;

  num_cameras = token.toInt();
  allocate_cameras(num_cameras);

  for (int i = 0; i < num_cameras; ++i) {
    QString filename;
    float focal_length;
    float orient[4]; // XYZW
    float center[3];
    float radial_distortion;
    int temp;

    stream >> filename;
    stream >> focal_length;
    stream >> orient[3] >> orient[0] >> orient[1] >> orient[2];
    stream >> center[0] >> center[1] >> center[2];
    stream >> radial_distortion;
    stream >> temp; // END of camera

    m_Cameras[i].focial_length = focal_length;
    m_Cameras[i]. // TODO
  }

  stream >> num_points;
}

void Bundle::allocate_cameras(int n)
{
  if (n > m_CamerasCapacity) {
    int cap = n;

    Camera* addr = (Camera*)malloc(sizeof(Camera) * cap);
    if (!addr) {
      // OUT OF MEMORY
      abort();
    }

    if (m_CamerasNum > 0) {
      Camera* src = m_Cameras;
      memcpy(addr, src, sizeof(Camera) * m_CamerasNum);
    }
    free(m_Cameras);

    (void)m_CamerasNum; // untouched
    m_CamerasCapacity = cap;
    m_Cameras = addr;
  }
}

}
