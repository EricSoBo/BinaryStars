#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glut.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>


GLuint texture[1];
static const double G = 1.0;
static double last_x = 0.0;
static const float dt = 0.01;
bool Pause = true;
GLfloat const red[3] = {1,0,0};
GLfloat const green[3] = {0,1,0};
GLfloat const blue[3] = {0,0,1};
GLfloat const gray[3] = {0.5,0.5,0.5};

int LoadBMP(const char * filename)
{
    #define SAIR {fclose(fp_arquivo); return -1;}
    #define CTOI(C) (*(int*)&C)
    GLubyte *image;
    GLubyte Header[0x54];
    GLuint DataPos, imageSize;
    GLsizei Width,Height;
    int nb = 0;
    // Abre o arquivo e efetua a leitura do Header do arquivo BMP
    FILE * fp_arquivo = fopen(filename,"rb");
    if (!fp_arquivo)
    return -1;
    if (fread(Header,1,0x36,fp_arquivo)!=0x36)
        SAIR;
    if (Header[0]!='B' || Header[1]!='M')
        SAIR;
    if (CTOI(Header[0x1E])!=0)
        SAIR;
    if (CTOI(Header[0x1C])!=24)
        SAIR;
    // Recupera a informação dos atributos de
    // altura e largura da imagem
    Width = CTOI(Header[0x12]);
    Height = CTOI(Header[0x16]);
    ( CTOI(Header[0x0A]) == 0 ) ? ( DataPos=0x36 ) : ( DataPos = CTOI(Header[0x0A]) );
    imageSize=Width*Height*3;
    // Efetura a Carga da Imagem
    image = (GLubyte *) malloc ( imageSize );
    int retorno;
    retorno = fread(image,1,imageSize,fp_arquivo);
    if (retorno !=imageSize)
    {
        free (image);
        SAIR;
    }
    // Inverte os valores de R e B
    int t, i;
    for ( i = 0; i < imageSize; i += 3 )
    {
        t = image[i];
        image[i] = image[i+2];
        image[i+2] = t;
    }
    // Tratamento da textura para o OpenGL
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexEnvf ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
    // Faz a geraçao da textura na memória
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Width, Height, 0, GL_RGB, GL_UNSIGNED_BYTE,
    image);
    fclose (fp_arquivo);
    free (image);
    return 1;
}

class Camera{
    public:
        Camera(): _pos(glm::vec3(0.0, 0.0, 0.0)), _dir(glm::vec3(0.0, 0.0, -1.0)), _left(glm::vec3(-1.0, 0.0, 0.0)), _up(glm::vec3(0.0, 1.0, 0.0)), _veloc(glm::vec3(0.0, 0.0, 0.0)), _ratio(0.01f), _yaw(0.0) {}
        Camera(glm::vec3 pos): _pos(pos), _dir(glm::vec3(0.0, 0.0, -1.0)), _left(glm::vec3(-1.0, 0.0, 0.0)), _up(glm::vec3(0.0, 1.0, 0.0)), _veloc(glm::vec3(0.0, 0.0, 0.0)), _ratio(0.01f), _yaw(0.0) {}
        void Look(){
            glm::vec3 _center = _pos + _dir;
            gluLookAt(_pos.x, _pos.y, _pos.z, _center.x, _center.y, _center.z, _up.x, _up.y, _up.z);
        }
        void Forward(){
            _veloc = _dir * _ratio;
            _pos += _veloc;
        }
        void Backward(){
            _veloc = (-_dir) * _ratio;
            _pos += _veloc;
        }
        void Left(){
            _veloc = (-_left) * _ratio;
            _pos += _veloc;
        }
        void Right(){
            _veloc = _left * _ratio;
            _pos += _veloc;
        }
        void UpdateYaw(GLfloat yaw){
            _yaw += yaw * _ratio;
            _dir.x = sin(_yaw);
            _dir.z = -cos(_yaw);
            glm::normalize(_dir);
            _left = glm::cross(_dir, _up);
        }
    private:
        glm::vec3 _pos;
        glm::vec3 _dir;
        glm::vec3 _left;
        glm::vec3 _up;
        glm::vec3 _veloc;
        GLfloat _ratio;
        GLfloat _yaw;

};

Camera camera(glm::vec3(0.0, 0.0, 0.0));

class Sphere{
    public:
        std::vector<glm::vec3> points;
        std::vector<std::vector<unsigned int>> indices;
        glm::vec3 _pos;

        Sphere(const std::string name, const double radius, const unsigned int sectorCount, const unsigned int stackCount, const glm::vec3 pos, const glm::vec3 velocity, const GLfloat color[], const double mass = 10.0, const bool make_trail = true){
            this->name = name;
            this->_pos = pos;
            this->mass = mass;
            this->radius = radius;
            this->color[0] = color[0];
            this->color[1] = color[1];
            this->color[2] = color[2];
            this->make_trail = make_trail;
            this->sectorCount = sectorCount;
            this->stackCount = stackCount;
            this->velocity = velocity;
            this->acceleration = glm::vec3(0.0, 0.0, 0.0);
            this->momentum =  (GLfloat)mass * velocity;
            //this->points = std::vector<glm::vec3>();
            //this->indices = std::vector<unsigned int>();
            double deltaPhi = M_PI / stackCount;
            double deltaTheta = 2 * M_PI / sectorCount;
            for(int i = 0; i <= stackCount; i++){
                std::vector<unsigned int> temp;
                double phi = i * deltaPhi;
                for(int j = 0; j <= sectorCount; j++){
                    double theta = j * deltaTheta;
                    double x = radius * sin(phi) * cos(theta);
                    double y = radius * sin(phi) * sin(theta);
                    double z = radius * cos(phi);
                    points.push_back(glm::vec3(x, y, z));
                    temp.push_back(points.size() - 1);
                }
                indices.push_back(temp);
            }
            std::cout << "Sphere " + name + " created" << std::endl;
        }

        ~Sphere(){
            std::cout << "Sphere destroyed" << std::endl;
        }

        void CalculateAcceleration(std::vector<Sphere*>& Entities){
            acceleration = glm::vec3(0.0, 0.0, 0.0);
            for(Sphere* Ent : Entities){
                if(Ent->_pos.x != _pos.x || Ent->_pos.y != _pos.y || Ent->_pos.z != _pos.z){
                    double distance = sqrt(pow(Ent->_pos.x - _pos.x, 2) + pow(Ent->_pos.y - _pos.y, 2) + pow(Ent->_pos.z - _pos.z, 2));
                    double force = (G * mass * Ent->GetMass()) / pow(distance, 2);
                    glm::vec3 direction(Ent->_pos.x - _pos.x, Ent->_pos.y - _pos.y, Ent->_pos.z - _pos.z);
                    direction = glm::normalize(direction);
                    acceleration += ((float)force / (float)mass) * direction;
                }
            }
            velocity += acceleration * dt;
        }

        void UpdatePosition(std::vector<Sphere*>& Entities){
            _pos.x += velocity.x * dt;
            _pos.y += velocity.y * dt;
            _pos.z += velocity.z * dt;
        }

        unsigned int GetStackCount(){
            return stackCount;
        }

        unsigned int GetSectorCount(){
            return sectorCount;
        }

        double GetRadius(){
            return radius;
        }

        double GetMass(){
            return mass;
        }

        void DrawSphere(){
            glPushMatrix();
            glColor3fv(color);
            glTranslated(_pos.x, _pos.y, _pos.z);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_CULL_FACE);
            glFrontFace(GL_CCW);
            glCullFace(GL_BACK);
            for(int i = 0; i < stackCount; i++){
                glBegin(GL_TRIANGLE_STRIP);
                for(int j = 0; j < sectorCount; j++){
                    unsigned int index = indices[i][j];
                    glVertex3fv(&points[index].x);
                    index = indices[i + 1][j];
                    glVertex3fv(&points[index].x);
                    if(j == sectorCount - 1){
                        index = indices[i][0];
                        glVertex3fv(&points[index].x);
                        index = indices[i + 1][0];
                        glVertex3fv(&points[index].x);
                    }
                }
                glEnd();
            }
            glPopMatrix();
            trail.push_back(_pos);
            for(int i = 0; i < trail.size(); i++){
                glPointSize(radius/2);
                glBegin(GL_POINTS);
                    glColor3f(color[0], color[1], color[2]);
                    glVertex3f(trail[i].x, trail[i].y, trail[i].z);
                glEnd();
            }
        }

    private:
        double radius;
        double mass;
        GLfloat color[3];
        std::string name;
        glm::vec3 momentum;
        glm::vec3 velocity;
        glm::vec3 acceleration;
        unsigned int sectorCount;
        unsigned int stackCount;
        std::vector<glm::vec3> trail;
        bool make_trail;
};

void DrawDistance(Sphere& s1, Sphere& s2){
    glLineWidth(10);
    glBegin(GL_LINES);
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(s1._pos.x, s1._pos.y, s1._pos.z);
        glVertex3f(s2._pos.x, s2._pos.y, s2._pos.z);
    glEnd();
    glPointSize(20);
    double x = (s1.GetMass()*s1._pos.x + s2.GetMass()*s2._pos.x)/(s1.GetMass() + s2.GetMass());
    double y = (s1.GetMass()*s1._pos.y + s2.GetMass()*s2._pos.y)/(s1.GetMass() + s2.GetMass());
    double z = (s1.GetMass()*s1._pos.z + s2.GetMass()*s2._pos.z)/(s1.GetMass() + s2.GetMass());
    glBegin(GL_POINTS);
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex3f(x, y, z);
    glEnd();
}

void Redimensiona(const int w, const int h){
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)w/(GLfloat)h, 0.1, 500.0);

    glMatrixMode(GL_MODELVIEW);
}

void GridLand(const int L = 500){
    float y = -0.5;
    glBegin(GL_LINES);
    glColor3f(0.0f, 1.0f, 0.0f);
    for(int i = -L; i <= L; i++){
        glVertex3f(i, y, -L);
        glVertex3f(i, y, L);
        glVertex3f(-L, y, i);
        glVertex3f(L, y, i);
    }
    glEnd();

}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    double xoffset = xpos - last_x;
    last_x = xpos;
    camera.UpdateYaw(xoffset);
}

void KeyEvents(GLFWwindow* window, Camera& camera){
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS){
        camera.Forward();
    }
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        camera.Backward();
    }
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){
        camera.Right();
    }
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
        camera.Left();
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if(key == GLFW_KEY_P && action == GLFW_PRESS){
        Pause = !Pause;
    }
}

int main(int argc, char** argv){
    std::vector<Sphere*> Entities;
    GLFWwindow* window;
    const int largura = 800;
    const int altura = 600;

    if(!glfwInit()){
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    window = glfwCreateWindow(largura, altura, "Binary Stars", NULL, NULL);
    glfwMakeContextCurrent(window);

    if(glewInit() != GLEW_OK){
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glutInit(&argc, argv);

    if(!window){
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, texture);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);

    Sphere Earth("Terra", 0.5, 50, 50, glm::vec3(0.0, 2.0, 0.0), glm::vec3(0.0, 0.0, 0.5), blue, 2.0);
    Sphere Mars("Marte", 0.2, 50, 50, glm::vec3(0.0, -2.0, 0.0), glm::vec3(0.0, 0.0, -0.5), red, 2.0);
    //Sphere CenterOfMass("CM", 0.0, 50, 50, glm::vec3(0.0, 3.0, 0.0), glm::vec3(0.0, 0.0, 0.0), 1000.0);
    Entities.push_back(&Mars);
    Entities.push_back(&Earth);

    while(!glfwWindowShouldClose(window)){
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        Redimensiona(largura, altura);
        camera.Look();
        //GridLand();
        Earth.DrawSphere();
        Mars.DrawSphere();
        if(!Pause){
            Earth.CalculateAcceleration(Entities);
            Mars.CalculateAcceleration(Entities);
            Earth.UpdatePosition(Entities);
            Mars.UpdatePosition(Entities);
        }
        DrawDistance(Mars, Earth);
        
        //Sun.DrawSphere();
        //Earth.DrawSphere();
        //std::cout << x << std::endl;
        KeyEvents(window, camera);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}