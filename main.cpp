#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "icelut_opengl_demo.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
float yaw=0;
float pitch =0;

// 顶点着色器，GLSL语言
const char *vertexShaderSource = "#version 330 core\n"
                                 "layout (location = 0) in vec3 aPos;\n"
                                 "//layout (location = 1) in vec3 aColor;\n"
                                 "layout (location = 1) in vec2 aTexCoord;\n"
                                 "out vec2 TexCoord;\n"
                                 "void main()\n"
                                 "{\n"
                                 "   gl_Position =vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                 "   TexCoord = vec2(aTexCoord.x, aTexCoord.y);\n"
                                 "}\0";
// 片元着色器
const char *fragmentShaderSource = "#version 330 core\n"
                                   "out vec4 FragColor;\n"
                                   "in vec2 TexCoord;\n"
                                   "uniform sampler2D texture1;\n"
                                   "void main()\n"
                                   "{\n"
                                 
                                   "   FragColor = texture(texture1, TexCoord);\n"
                                   "}\n\0";


std::mutex g_glfwMutex;

void glDraw(std::string windowname,bool useLUT=false)
{
int width, height, nrChannels;
//int width=720;
//int height=558;
//int nrChannels=3;
g_glfwMutex.lock();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window1 creation
    GLFWwindow* window1 = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, windowname.c_str(), NULL, NULL);
    if (window1 == NULL)
    {
        std::cout << "Failed to create GLFW window1" << std::endl;
        g_glfwMutex.unlock();
        glfwTerminate();
        return;
    }
 

    glfwMakeContextCurrent(window1);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        g_glfwMutex.unlock();
        return;
    }
     g_glfwMutex.unlock();

    glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    // vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // fragment shader
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // link shaders
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);


    float vertices[] = {

     1.0f,  1.0f, 0.0f,   1.0f, 1.0f,   // 右上
     1.0f, -1.0f, 0.0f,   1.0f, 0.0f,   // 右下
    -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,   // 左下
    -1.0f,  1.0f, 0.0f,   0.0f, 1.0f    // 左上

    };


unsigned int indices[] = {
    0, 1, 3, // 第一个三角形
    1, 2, 3  // 第二个三角形
};



 
    unsigned int VBO, VAO,EBO;
    //创建VAO对象
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    //创建VBO对象，把顶点数组复制到一个顶点缓冲中，供OpenGL使用
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // 缓冲绑定到GL_ARRAY_BUFFER
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // 顶点数据复制到缓冲的内存中
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load("../test.jpg", &width, &height, &nrChannels, 0);

    if (!data)
    {

        std::cout << "Open failed!" << std::endl;
    }
    else
    {

        std::cout << "Open Success!" << std::endl;
    }
    //纹理ID
    unsigned int texture1;
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);    

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    if(useLUT){
        static icelut_demo::IcelutOpenGLProcessor processor;
        texture1 = processor.processTextureToNewTexture(texture1,width,height);
    }

    glUseProgram(shaderProgram);

    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0); 

    //解释顶点数据方式
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); // 顶点数据的解释
      glEnableVertexAttribArray(0);

    //纹理属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);



    // 解绑VAO
    glBindVertexArray(0);
    // 解绑VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    while (!glfwWindowShouldClose(window1))
    {

        glClearColor(0.0f, 0.1f, 0.5f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_BLEND);

        glUseProgram(shaderProgram);


        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);  // 改为使用索引绘制
             g_glfwMutex.lock();
        glfwSwapBuffers(window1);
        glfwPollEvents();
         g_glfwMutex.unlock();

    }

    // optional: de-allocate all resources
    std::cout << "stop!" << std::endl;
    g_glfwMutex.lock();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
     glfwDestroyWindow(window1);
     g_glfwMutex.unlock();
}
int main()
{
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    std::thread threadorignal(&glDraw,"OriginalImage",false);
    std::thread threadlut(&glDraw,"LUTImage",true);
    threadorignal.join();
    threadlut.join();
    glfwTerminate();
    return 0;
}


