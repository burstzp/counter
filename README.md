##手机归属地查询扩展
#一、扩展安装: 
* 1. 解压mobile2loc.zip 
* 2. /configure 
* 3. make && make install 
* 将数据文件上传到指定目录，apache或者nginx 用户 有读取权限 
* 配置 php.ini 文件 
* 如:

```
[mobile2loc]
extension=mobile2loc.so 
mobile2loc.filename="/path/mobile2loc.dat" 
```
* 效果如下:
```
[root@lamp9 test]# php -r "var_dump(mobile2loc(15010063546));" 
string(43) "北京 北京 北京移动150卡 010 100000"
```
  
#二、压测数据:
环境：1G, 虚拟机 
```
Total: 276849 
Consume Time: 9.646087884903 
Consume Mem: 784字节 
约：28689 个/s 
```
