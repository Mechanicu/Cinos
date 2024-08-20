# deploy OBS on openEuler
## OBS deployment
### 1 oE尝试部署OBS
#### 1.1 httpd的拉起失败
OBS需要一个http前端帮助用户使用OBS，其实现依赖于httpd.service的启动，但是OBS的配置文件无法直接在oE上启动，主要有以下问题

**1.缺少模块**

缺少mod可能有两个原因：
  - mod未安装：大部分mod以mod_@module-name的形式提供，可以通过yum安装。可以在/etc/httpd/modules目录下查看当前安装的所有apache server modules
  - mod安装了，但是未加载（load）:httpd/Apache Server有两类config文件
    - Apache server的conf文件在/etc/httpd/conf.d/目录下
    - 指定Apache server需要加载的module的conf文件在/etc/httpd/conf.modules.d目录下，通过
    ```
    LoadModule @module_name @module_path
    ```
    命令实现module的加载，并且可以通过
    ```
    apachectl -M | grep @module_name
    ```
    查看module加载是否成功
    
比如
  - 未安装mod_ssl导致无法使用SSLengine命令，该mod安装之后会自动load
  - mod_xforward已安装，但是mod没有自动load，需要手动添加mod conf文件是实现load mod

**2.httpd无法监听指定端口**

主要原因是缺少SELinux permissions.SELinux默认只允许apache server/httpd绑定到几个可选的port：
```
80, 81, 443, 488, 8008, 8009, 8443, 9000
```
而OBS服务需要httpd监听82端口，没有SELinux权限，需要通过以下命令添加权限
```
@-t http_port_t:指定对httpd服务添加权限
@-p tcp:使用TCP协议
@listend_port:httpd服务监听的端口
semanage port -m -t http_port_t -p tcp @listend_port
```