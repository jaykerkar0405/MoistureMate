import { json, type RequestHandler } from '@sveltejs/kit';
import { device, isConnected, CONTROL_TOPIC, type CommandPayload } from '$lib/iot-store';

export const POST: RequestHandler = async ({ request }) => {
	try {
		const body: CommandPayload = await request.json();
		const { command, state, value } = body;

		if (!command) {
			return json({ error: 'Missing required command parameter' }, { status: 400 });
		}

		if (!device || !isConnected) {
			return json({ error: 'IoT device not connected' }, { status: 503 });
		}

		const message = {
			command,
			state,
			value: value ? parseInt(value.toString()) : undefined
		};

		// Promisify the publish method
		await new Promise<void>((resolve, reject) => {
			device?.publish(CONTROL_TOPIC, JSON.stringify(message), {}, (err) => {
				if (err) reject(err);
				else resolve();
			});
		});

		console.log(`Command sent to ${CONTROL_TOPIC}:`, message);

		return json({
			success: true,
			message: `Command ${command} sent successfully`
		});
	} catch (error) {
		console.error('Error sending command:', error);
		return json(
			{
				error: 'Internal server error',
				details: (error as Error).message
			},
			{ status: 500 }
		);
	}
};
