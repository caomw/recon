#include <Bundle.h>
#include <OpenGLWindow.h>
#include <QtCore>
#include <QtDebug>
#include <QGuiApplication>
#include <QOpenGLShaderProgram>

class BundleWindow : public recon::OpenGLWindow {
public:
  BundleWindow();

  void initialize() override;
  void render() override;

private:
  GLuint m_FeatureVBO;
  GLint m_ModelViewMatrixUniform;
  GLint m_CalibrationMatrixUniform;

  QOpenGLShaderProgram *m_Program;
  recon::Bundle m_Bundle;
};

BundleWindow::BundleWindow()
{
}

void BundleWindow::initialize()
{
  // Load bundle
  {
    QFile file("data/e100vs/bundle.nvm");
    file.open(QIODevice::ReadOnly);
    m_Bundle.load_nvm(&file);
    file.close();
  }
  qDebug() << m_Bundle.camera_count() << " cameras";
  qDebug() << m_Bundle.feature_count() << " points";

  const char* version = (const char*) glGetString(GL_VERSION);
  qDebug() << "OpenGL Version = " << version;

  // Copy to vertex buffer
  glGenBuffers(1, &m_FeatureVBO);
  glBindBuffer(GL_ARRAY_BUFFER, m_FeatureVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(recon::FeatureVertex) * m_Bundle.feature_count(), m_Bundle.get_features(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Load Shader
  m_Program = new QOpenGLShaderProgram(this);
  {
    QFile file("shader/bundle_feature.vert");
    file.open(QIODevice::ReadOnly);
    if (!m_Program->addShaderFromSourceCode(QOpenGLShader::Vertex, file.readAll())) {
      qDebug() << "Failed to compile vertex shader:\n" << m_Program->log();
    }
    file.close();
  }
  {
    QFile file("shader/bundle_feature.frag");
    file.open(QIODevice::ReadOnly);
    if (!m_Program->addShaderFromSourceCode(QOpenGLShader::Fragment, file.readAll())) {
      qDebug() << "Failed to compile fragment shader:\n" << m_Program->log();
    }
    file.close();
  }
  m_Program->bindAttributeLocation("in_Position", 0);
  m_Program->bindAttributeLocation("in_Color", 1);
  if (!m_Program->link()) {
    qDebug() << "Failed to link program:\n" << m_Program->log();
  }

  m_ModelViewMatrixUniform = m_Program->uniformLocation("u_ModelViewMatrix");
  m_CalibrationMatrixUniform = m_Program->uniformLocation("u_CalibrationMatrix");
}

void BundleWindow::render()
{
  using vectormath::aos::vec3;
  using vectormath::aos::vec4;
  using vectormath::aos::mat3;
  using vectormath::aos::mat4;
  using vectormath::aos::load_vec3;
  using vectormath::aos::load_mat3;
  using vectormath::aos::store_mat3;
  using vectormath::aos::store_mat4;

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const recon::Camera& cam = m_Bundle.get_camera(0);

  m_Program->bind();
  {
    GLfloat buf[4][4];

    mat3 rot = load_mat3(cam.extrinsic);
    vec3 trans = load_vec3(cam.extrinsic+9);
    mat4 modelview = make_mat4(rot, trans);
    store_mat4((GLfloat*)buf, modelview);

    m_Program->setUniformValue(m_ModelViewMatrixUniform, buf);
  }
  {
    GLfloat buf[3][3];
    mat3 m1 = load_mat3(cam.intrinsic);

    store_mat3((GLfloat*)buf, m1);
    m_Program->setUniformValue(m_CalibrationMatrixUniform, buf);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_FeatureVBO);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(recon::FeatureVertex), (void*)offsetof(recon::FeatureVertex, pos));
  glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(recon::FeatureVertex), (void*)offsetof(recon::FeatureVertex, color));
  glDrawArrays(GL_POINTS, 0, m_Bundle.feature_count());

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_Program->release();
}

int main(int argc, char* argv[])
{
  QGuiApplication app(argc, argv);

  QSurfaceFormat format;
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setRedBufferSize(8);
  format.setGreenBufferSize(8);
  format.setBlueBufferSize(8);
  format.setAlphaBufferSize(8);
  format.setDepthBufferSize(24);

  BundleWindow window;
  window.setFormat(format);
  window.resize(800, 600);
  window.show();

  return app.exec();
}
