<script lang="ts">
	import { SignIn } from '@auth/sveltekit/components';
</script>

<div>
	<img src="/img/logo.svg" alt="Company Logo" />
	<SignIn provider="google" signInPage="signin" />
	<SignIn provider="github" signInPage="signin" />
</div>
