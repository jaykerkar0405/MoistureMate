import { json, type RequestHandler } from '@sveltejs/kit';
import { isConnected, latestStatusMessage } from '$lib/iot-store';

export const GET: RequestHandler = async () => {
	return json({
		status: isConnected ? 'connected' : 'disconnected',
		latestMessage: latestStatusMessage || { message: 'No status updates yet' }
	});
};
