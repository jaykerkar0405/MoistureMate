import ora from 'ora';
import path from 'path';
import chalk from 'chalk';
import awsIot, { type device as IoTDevice } from 'aws-iot-device-sdk';

function getCertDir() {
	return path.join(process.cwd(), 'certs');
}

const CERT_DIR = getCertDir();

export interface LatestMessage {
	topic: string;
	payload: unknown;
	timestamp: Date;
}

export interface CommandPayload {
	value?: number;
	state?: string;
	command: string;
}

export let isConnected = false;
export let device: IoTDevice | null = null;
export let latestStatusMessage: LatestMessage | null = null;

export const STATUS_TOPIC = 'plant/test/status';
export const CONTROL_TOPIC = 'plant/test/control';

const iotConfig: awsIot.DeviceOptions = {
	protocol: 'mqtts',
	region: 'ap-south-1',
	caPath: path.join(CERT_DIR, 'rootCA.pem'),
	keyPath: path.join(CERT_DIR, 'private.key'),
	certPath: path.join(CERT_DIR, 'certificate.pem'),
	host: 'awdwy51wq89pl-ats.iot.ap-south-1.amazonaws.com',
	clientId: `svelte-app-${Math.floor(Math.random() * 100000)}`
};

export function initializeDevice(): void {
	if (device) return;

	const spinner = ora({
		text: chalk.blue('Connecting to AWS IoT Core...'),
		spinner: 'dots'
	}).start();

	try {
		device = new awsIot.device(iotConfig);

		device.on('connect', () => {
			spinner.succeed(chalk.green('Connected to AWS IoT Core'));
			isConnected = true;

			const subSpinner = ora({
				text: chalk.blue(`Subscribing to topics: ${[STATUS_TOPIC, CONTROL_TOPIC].join(', ')}`),
				spinner: 'dots'
			}).start();

			device?.subscribe([STATUS_TOPIC, CONTROL_TOPIC], { qos: 1 }, (err, granted) => {
				if (err) {
					subSpinner.fail(chalk.red('Error subscribing to topics'));
					console.error(chalk.red('Subscription error details:'), err);
				} else {
					subSpinner.succeed(
						chalk.green(`Subscribed to topics: ${granted?.map((g) => g.topic).join(', ')}`)
					);
				}
			});
		});

		device.on('message', (topic: string, payload: Buffer) => {
			try {
				const message = JSON.parse(payload.toString());

				const topicColor = topic === STATUS_TOPIC ? chalk.cyan : chalk.yellow;
				console.log(
					`${chalk.gray('[')}${new Date().toISOString()}${chalk.gray(']')} ${topicColor(topic)}:`
				);
				console.dir(message, { colors: true, depth: null });

				if (topic === STATUS_TOPIC) {
					latestStatusMessage = {
						topic,
						payload: message,
						timestamp: new Date()
					};
				}
			} catch (error) {
				console.error(chalk.red('Error parsing IoT message:'), error);
			}
		});

		device.on('error', (err) => {
			spinner.fail(chalk.red('IoT device error'));
			console.error(chalk.red('Connection error details:'), err);
			isConnected = false;
		});

		device.on('offline', () => {
			console.log(chalk.yellow('⚠️ Device is offline'));
			isConnected = false;
		});

		device.on('reconnect', () => {
			const reconnectSpinner = ora({
				text: chalk.blue('Reconnecting to AWS IoT Core...'),
				spinner: 'dots'
			}).start();

			device?.on('connect', () => {
				reconnectSpinner.succeed(chalk.green('Reconnected to AWS IoT Core'));
				isConnected = true;
			});
		});
	} catch (error) {
		spinner.fail(chalk.red('Error initializing AWS IoT device'));
		console.error(chalk.red('Initialization error details:'), error);
		throw error;
	}
}

export function publishCommand(command: CommandPayload): void {
	if (!device || !isConnected) {
		console.error(chalk.red('Cannot publish: Device not connected'));
		return;
	}

	const cmdSpinner = ora({
		text: chalk.blue(`Publishing command to ${CONTROL_TOPIC}`),
		spinner: 'dots'
	}).start();

	try {
		device.publish(CONTROL_TOPIC, JSON.stringify(command), { qos: 1 }, (err) => {
			if (err) {
				cmdSpinner.fail(chalk.red('Failed to publish command'));
				console.error(chalk.red('Publish error details:'), err);
			} else {
				cmdSpinner.succeed(chalk.green(`Command published: ${command.command}`));
			}
		});
	} catch (error) {
		cmdSpinner.fail(chalk.red('Error publishing command'));
		console.error(chalk.red('Publish error details:'), error);
	}
}

try {
	initializeDevice();
} catch (error) {
	console.error(chalk.red('Failed to initialize IoT device on startup:'), error);
}
