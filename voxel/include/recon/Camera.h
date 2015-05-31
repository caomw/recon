#pragma once

#include <vectormath.h>
#include <QSharedDataPointer>
#include <QString>

namespace recon {

using vectormath::aos::vec3;
using vectormath::aos::vec4;
using vectormath::aos::quat;
using vectormath::aos::mat3;
using vectormath::aos::mat4;

struct CameraData;

class Camera {
public:
  Camera();
  Camera(const Camera&);
  ~Camera();
  Camera& operator=(const Camera&);

public:
  struct RadialDistortion {
    float k1;
    float k2;
  };

  float focal() const;
  void setFocal(float focal);

  float aspect() const;
  void setAspect(float aspect);

  RadialDistortion distortion() const;
  void setRadialDistortion(const RadialDistortion&);
  void setRadialDistortion(float, float);

  vec3 center() const;
  void setCenter(vec3);

  mat3 rotation() const;
  void setRotation(mat3);
  void setRotation(quat);

  mat4 extrinsic() const;
  mat4 intrinsicForViewport() const;
  mat4 intrinsicForImage(int width, int height) const;

  QString imagePath() const;
  void setImagePath(const QString&);

  QString maskPath() const;
  void setMaskPath(const QString&);

  bool canSee(vec3 pt) const;

private:
  //QSharedDataPointer<CameraData> data;
  QExplicitlySharedDataPointer<CameraData> data;
};

// TODO: undistortion

}

Q_DECLARE_TYPEINFO(recon::Camera, Q_MOVABLE_TYPE);
