use vulkano::device::physical::PhysicalDevice;
use vulkano::device::{Device, DeviceCreateInfo, QueueCreateInfo};
use vulkano::instance::{Instance, InstanceCreateInfo};

fn main() {
    let instance = Instance::new(InstanceCreateInfo::default()).unwrap();
    for physical_device in PhysicalDevice::enumerate(&instance) {
        println!(
            "Found Physical Device: {}",
            physical_device.properties().device_name
        );
    }
    let physical = PhysicalDevice::enumerate(&instance)
        .next()
        .expect("no device available");
    println!(
        "Physical Device Chosen: {}",
        physical.properties().device_name
    );

    for family in physical.queue_families() {
        println!(
            "Found a queue family with {:?} queue(s)",
            family.queues_count()
        );
    }

    let queue_family = physical
        .queue_families()
        .find(|&q| q.supports_graphics())
        .expect("couldn't find a graphical queue family");

    let (device, mut queues) = Device::new(
        physical,
        DeviceCreateInfo {
            // here we pass the desired queue families that we want to use
            queue_create_infos: vec![QueueCreateInfo::family(queue_family)],
            ..Default::default()
        },
    )
    .expect("failed to create device");
}
