use std::env;

use tonic::{transport::Server, Request, Response, Status};
use hello::greeter_server::{Greeter, GreeterServer};
use hello::{HelloRequest, HelloReply};
pub mod hello{
    tonic::include_proto!("sayhello");
}

#[derive(Default)]
pub struct MyHello {}

#[tonic::async_trait]
impl Greeter for MyHello {
    async fn say_hello(
        &self,
        request:Request<HelloRequest>
    )->Result<Response<HelloReply>,Status>
    {
        println!("Got request {:?}", request);
        let msg = format!("Hello {}", request.get_ref().name);
        Ok(Response::new(HelloReply{
            message:msg,
            pid: std::process::id() as i32,
        }))
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>>
{
    let args: Vec<String> = env::args().collect();
    let addr = format!("[::]:{}", args[1]).parse()?;
    let server = MyHello::default();
    Server::builder()
        .add_service(GreeterServer::new(server)).
        serve(addr).
        await?;
    Ok(())
}
