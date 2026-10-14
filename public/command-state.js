// Internal commands live in the state snapshot; HTTP commands require their ACK
// endpoint. Missing data never implies completion or an energized physical valve.
export function observedCommand(device,id,httpStatuses=[]){
 if(!device||!id)return null;
 return device.transport==='http'?httpStatuses.find(command=>command.command_id===id)||null:device.commands?.[id]||null;
}
