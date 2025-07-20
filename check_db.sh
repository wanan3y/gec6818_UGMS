#!/bin/bash

echo "=== 数据库状态检查 ==="
echo "当前时间: $(date)"
echo ""

# 检查数据库文件
echo "1. 数据库文件状态:"
if [ -f "/mnt/udisk/Carsystem/parking.db" ]; then
    echo "   数据库文件存在"
    echo "   文件大小: $(ls -lh /mnt/udisk/Carsystem/parking.db | awk '{print $5}')"
    echo "   文件权限: $(ls -l /mnt/udisk/Carsystem/parking.db | awk '{print $1}')"
    echo "   所有者: $(ls -l /mnt/udisk/Carsystem/parking.db | awk '{print $3}')"
    echo "   修改时间: $(ls -l /mnt/udisk/Carsystem/parking.db | awk '{print $6, $7, $8}')"
else
    echo "   数据库文件不存在"
fi
echo ""

# 检查目录权限
echo "2. 目录权限:"
if [ -d "/mnt/udisk/Carsystem" ]; then
    echo "   目录存在"
    echo "   目录权限: $(ls -ld /mnt/udisk/Carsystem | awk '{print $1}')"
    echo "   所有者: $(ls -ld /mnt/udisk/Carsystem | awk '{print $3}')"
else
    echo "   目录不存在"
fi
echo ""

# 尝试查询数据库内容
echo "3. 数据库内容:"
if [ -f "/mnt/udisk/Carsystem/parking.db" ]; then
    echo "   当前数据库中的所有车辆:"
    sqlite3 /mnt/udisk/Carsystem/parking.db "SELECT * FROM info;" 2>/dev/null || echo "   无法读取数据库或表不存在"
    
    echo "   表结构:"
    sqlite3 /mnt/udisk/Carsystem/parking.db ".schema" 2>/dev/null || echo "   无法读取表结构"
    
    echo "   检查特定车牌 '贵A61000':"
    sqlite3 /mnt/udisk/Carsystem/parking.db "SELECT COUNT(*) FROM info WHERE 车牌='贵A61000';" 2>/dev/null || echo "   查询失败"
else
    echo "   数据库文件不存在，无法查询"
fi
echo ""

echo "=== 检查完成 ===" 