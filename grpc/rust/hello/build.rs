fn main() -> Result<(), Box<dyn std::error::Error>> {
    tonic_build::compile_protos("src/protos/hello.proto")?;
    Ok(())
}
