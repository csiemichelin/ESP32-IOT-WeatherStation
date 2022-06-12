var http = require('http');
var path = require('path');
var fs = require('fs');

// 修改port
var port = process.env.PORT || 8016; //8345;
// ESP32 server
var esp32_req = "http://192.168.43.98/temp";

//啟動網路伺服器並處理HTPP請求
var srv = http.createServer(function (req, res) {

    var filePath = '.' + req.url;
    if (filePath == './')
        filePath = './weather.html';

    var extname = path.extname(filePath);
    var contentType = 'text/html';
    switch (extname) {
        case '.js':
            contentType = 'text/javascript';
            break;
        case '.css':
            contentType = 'text/css';
            break;
    }

    fs.exists(filePath, function(exists) {

        if (exists) {
            fs.readFile(filePath, function(error, content) {
                if (error) {
                    res.writeHead(500);
                    res.end();
                } else {
                    res.writeHead(200, {
                        'Content-Type' : contentType
                    });
                    res.end(content, 'utf-8');
                }
            });
        } else {
            res.writeHead(404);
            res.end();
        }
    });

});


gw_srv = require('socket.io')(srv);
srv.listen(port);
console.log('Server running at http://127.0.0.1:' + port +'/');

gw_srv.sockets.on('connection', function(socket) {
    var dataPusher = setInterval(function () {
        //socket.volatile.emit('data', Math.random() * 100);
        http.get(esp32_req, (resp) => {
        let data = '';

        // 已經收到一段資料
        resp.on('data', (chunk) => {
            data += chunk;
        });

        // 已經收到完整回應，將結果顯示出來
        resp.on('end', () => {
            console.log('Received data: ',data);
            socket.volatile.emit('data', data);
            //console.log(JSON.parse(data).explanation);
        });

        }).on("error", (err) => {
        console.log("Error: " + err.message);
        });
    }, 2000);


    socket.on('disconnect', function() {
        console.log('closing');
        //gw_srv.close();
        srv.close();
    });


}); // on connection