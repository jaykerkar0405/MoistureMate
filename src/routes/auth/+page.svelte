<script lang="ts">
	import { SignIn } from '@auth/sveltekit/components';
</script>

<div class="flex min-h-screen items-center justify-center p-4">
	<div class="w-full max-w-md overflow-hidden rounded-xl bg-gray-800 shadow-lg">
		<div class="p-8">
			<div class="mb-8 text-center">
				<h1 class="mb-2 text-3xl font-bold">Welcome back</h1>
				<p class="text-gray-400">Sign in to your account to continue</p>
			</div>

			<div class="space-y-4">
				<button
					class="flex w-full cursor-pointer items-center justify-center gap-3 rounded-lg border border-gray-600 bg-gray-700 px-4 py-3 font-medium text-gray-200 hover:bg-gray-600"
				>
					<SignIn provider="google">
						<div slot="submitButton" class="flex cursor-pointer items-center gap-3">
							<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="20" height="20">
								<path
									fill="#4285F4"
									d="M44.5 20H24v8.5h11.8c-1.7 5-6.3 8.5-11.8 8.5-7 0-12.5-5.5-12.5-12.5S17 12 24 12c3 0 5.7 1 7.9 2.9l6.3-6.3C34.4 4.3 29.5 2 24 2 11.8 2 2 11.8 2 24s9.8 22 22 22c10.5 0 21-7.5 21-22 0-1.3-.2-2.7-.5-4z"
								/>
								<path
									fill="#34A853"
									d="M6.3 14.7l6.6 4.9C15 16.3 19.2 14 24 14c3 0 5.7 1 7.9 2.9l6.3-6.3C34.4 4.3 29.5 2 24 2 15.5 2 8.3 7 6.3 14.7z"
								/>
								<path
									fill="#FBBC05"
									d="M24 46c5.8 0 10.7-1.9 14.7-5.1l-7.3-5.8C28.9 36 26.6 37 24 37c-5.5 0-10-3.5-11.8-8.5l-6.6 5.1C8.3 41 15.5 46 24 46z"
								/>
								<path
									fill="#EA4335"
									d="M44.5 20H24v8.5h11.8c-.8 2.5-2.3 4.7-4.3 6.2l7.3 5.8c2.6-2.4 4.5-5.8 5.2-9.6.3-1.6.5-3.2.5-4.9 0-1.3-.2-2.7-.5-4z"
								/>
							</svg>
							<span>Sign in with Google</span>
						</div>
					</SignIn>
				</button>

				<button
					class="flex w-full cursor-pointer items-center justify-center gap-3 rounded-lg border border-gray-600 bg-gray-700 px-4 py-3 font-medium text-gray-200 hover:bg-gray-600"
				>
					<SignIn provider="github">
						<div slot="submitButton" class="flex cursor-pointer items-center gap-3">
							<svg
								xmlns="http://www.w3.org/2000/svg"
								width="20"
								height="20"
								viewBox="0 0 24 24"
								fill="none"
								stroke="currentColor"
								stroke-width="2"
								stroke-linecap="round"
								stroke-linejoin="round"
								class="text-white"
							>
								<path
									d="M9 19c-5 1.5-5-2.5-7-3m14 6v-3.87a3.37 3.37 0 0 0-.94-2.61c3.14-.35 6.44-1.54 6.44-7A5.44 5.44 0 0 0 20 4.77 5.07 5.07 0 0 0 19.91 1S18.73.65 16 2.48a13.38 13.38 0 0 0-7 0C6.27.65 5.09 1 5.09 1A5.07 5.07 0 0 0 5 4.77a5.44 5.44 0 0 0-1.5 3.78c0 5.42 3.3 6.61 6.44 7A3.37 3.37 0 0 0 9 18.13V22"
								></path>
							</svg>
							<span>Sign in with GitHub</span>
						</div>
					</SignIn>
				</button>
			</div>
		</div>
	</div>
</div>
